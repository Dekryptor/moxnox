module;

#include <span>
#include <optional>
#include <functional>
#include <string_view>
#include <variant>
#include "thread.h"
#include "file.h"
#include "section.h"

export module visit;

import event;
import pipe;
import volume;
import semaphore;
import chardev;
import timer;

using namespace std;

// FIXME - concept to make sure func is invocable(?)
template<typename T>
static void visit_obj(object& obj, const T& func) {
    switch (obj.type) {
        case object_type::thread:
            func(static_cast<thread&>(obj));
        break;

        case object_type::timer:
            func(static_cast<timer&>(obj));
        break;

        case object_type::event:
            func(static_cast<event&>(obj));
        break;

        case object_type::chardev:
            func(static_cast<chardev&>(obj));
        break;

        case object_type::semaphore:
            func(static_cast<semaphore&>(obj));
        break;

        case object_type::volume:
            func(static_cast<volume&>(obj));
        break;

        case object_type::inode:
            func(static_cast<inode&>(obj));
        break;

        case object_type::process:
            func(static_cast<process&>(obj));
        break;

        case object_type::section:
            func(static_cast<section&>(obj));
        break;

        case object_type::file:
            func(static_cast<file&>(obj));
        break;
    }
}

export void object_acquire(object& obj) {
    visit_obj(obj, [](auto&& o) {
        o.acquire();
    });
}

export void object_release(object& obj) {
    visit_obj(obj, [](auto&& o) {
        o.release();
    });
}

export bool object_is_sync_object(object& obj) {
    bool ret = false;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_sync_object<remove_reference_t<decltype(o)>>)
            ret = true;
        else if constexpr (has_get_sync_object<remove_reference_t<decltype(o)>>)
            ret = true;
    });

    return ret;
}

export void object_on_wake(object& obj) {
    visit_obj(obj, [&](auto&& o) {
        if constexpr (object_has_on_wake<remove_reference_t<decltype(o)>>)
            o.on_wake();
    });
}

export nt_status object_write_without_offset(object& obj, span<const uint8_t> buf,
                                             uint64_t& written) {
    nt_status status;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_writable_object_without_offset<remove_reference_t<decltype(o)>>)
            status = o.write(buf, written);
        else {
            status = nt_status::NOT_SUPPORTED;
            written = 0;
        }
    });

    return status;
}

export nt_status object_write_with_offset(object& obj, span<const uint8_t> s,
                                          uint64_t offset, uint64_t& written_bytes) {
    nt_status status;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_writable_object_with_offset<remove_reference_t<decltype(o)>>)
            status = o.write(s, offset, written_bytes);
        else
            status = nt_status::NOT_SUPPORTED;
    });

    return status;
}

export nt_status object_ioctl(object& obj, uint32_t control_code, span<const uint8_t> inbuf,
                              span<uint8_t> outbuf, IO_STATUS_BLOCK& iosb) {
    nt_status ret;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (has_ioctls<decltype(o)>)
            ret = o.ioctl(control_code, inbuf, outbuf, iosb.Information);
        else if constexpr (is_same_v<remove_reference_t<decltype(o)>, file>) {
            visit_obj(o.ino, [&](auto&& o) {
                if constexpr (has_ioctls<decltype(o)>)
                    ret = o.ioctl(control_code, inbuf, outbuf, iosb.Information);
                else
                    ret = nt_status::NOT_SUPPORTED;
            });
        } else
            ret = nt_status::NOT_SUPPORTED;
    });

    return ret;
}

export nt_status object_read_with_offset(object& obj, span<uint8_t> s,
                                         uint64_t offset, uint64_t& read_bytes) {
    nt_status status;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_readable_object_with_offset<remove_reference_t<decltype(o)>>)
            status = o.read(s, offset, read_bytes);
        else
            status = nt_status::NOT_SUPPORTED;
    });

    return status;
}

export nt_status object_read_without_offset(object& obj, span<uint8_t> s,
                                            uint64_t& read_bytes) {
    nt_status status;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_readable_object_without_offset<remove_reference_t<decltype(o)>>)
            status = o.read(s, read_bytes);
        else
            status = nt_status::NOT_SUPPORTED;
    });

    return status;
}

export bool object_is_exclusive(object& obj) {
    bool ret;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (has_exclusive_field<remove_reference_t<decltype(o)>>)
            ret = o.exclusive;
        else
            ret = false;
    });

    return ret;
}

export sync_object& object_get_sync_object(object& obj) {
    sync_object* so = nullptr;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (has_get_sync_object<remove_reference_t<decltype(o)>>)
            so = &o.get_sync_object();
        else if constexpr (is_sync_object<remove_reference_t<decltype(o)>>)
            so = &static_cast<sync_object&>(o);
    });

    return *so;
}

export void object_last_handle_closed(object& obj) {
    visit_obj(obj, [&](auto&& o) {
        if constexpr (has_last_handle_closed<remove_reference_t<decltype(o)>>)
            o.last_handle_closed();
    });
}
