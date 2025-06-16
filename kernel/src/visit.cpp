module;

#include "kernel.h"
#include "spinlock.h"
#include "handle.h"
import rbtree;
import event;
import pipe;
import timer;
import objects;
#include "thread.h"
import page_cache;
import rwsem;
#include "inode.h"
import volume;
#include "visit_object.h"

export module visit;

using namespace std;

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
