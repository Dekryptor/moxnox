module;

#include <atomic>
#include <span>
#include <string_view>

export module objects;

import nt;
import list;
import format;

using namespace std;

export enum class object_type {
    thread,
    timer,
    event,
    chardev,
    semaphore,
    volume,
    inode,
    process,
    section,
    file
};

template<>
consteval bool format_spec_valid<object_type>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, object_type arg) {
    switch (arg) {
        using enum object_type;

        case thread:
            return do_format_arg(sp, "", "thread");
        case timer:
            return do_format_arg(sp, "", "timer");
        case event:
            return do_format_arg(sp, "", "event");
        case chardev:
            return do_format_arg(sp, "", "chardev");
        case semaphore:
            return do_format_arg(sp, "", "semaphore");
        case volume:
            return do_format_arg(sp, "", "volume");
        case inode:
            return do_format_arg(sp, "", "inode");
        case process:
            return do_format_arg(sp, "", "process");
        case section:
            return do_format_arg(sp, "", "section");
        case file:
            return do_format_arg(sp, "", "file");
        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

export
extern "C++"
class object {
public:
    object(enum object_type type) : type(type) { }

    enum object_type type;
    atomic<unsigned int> refcount = 1;
    atomic<unsigned int> handle_count = 0;
    bool named = false;
    bool persistent = false;
};

export
extern "C++"
class sync_object : public object {
public:
    sync_object(object_type type, bool signalled) : object(type), signalled(signalled) { }

    bool signalled;
    list_entry waiters = { &waiters, &waiters };
};

export
template<typename T>
concept is_sync_object = derived_from<T, sync_object>;

export
template<typename T>
concept object_has_on_wake = requires(T t) {
    { t.on_wake() };
};

export
template<typename T>
concept is_writable_object_with_offset = requires(T t, span<const uint8_t> s, uint64_t offset, uint64_t& written_bytes) {
    { t.write(s, offset, written_bytes) } -> convertible_to<NTSTATUS>;
};

export
template<typename T>
concept is_writable_object_without_offset = requires(T t, span<const uint8_t> s, uint64_t& written_bytes) {
    { t.write(s, written_bytes) } -> convertible_to<NTSTATUS>;
};

// FIXME - version of this specifying offset
// FIXME - async reads and writes (specifying event)
export
template<typename T>
concept is_readable_object_with_offset = requires(T t, span<uint8_t> s, uint64_t offset, uint64_t& read_bytes) {
    { t.read(s, offset, read_bytes) } -> convertible_to<NTSTATUS>;
};

export
template<typename T>
concept is_readable_object_without_offset = requires(T t, span<uint8_t> s, uint64_t& read_bytes) {
    { t.read(s, read_bytes) } -> convertible_to<NTSTATUS>;
};

export
template<typename T>
concept has_ioctls = requires(T t, uint32_t code, span<const uint8_t> inbuf, span<uint8_t> outbuf, uintptr_t& written) {
    { t.ioctl(code, inbuf, outbuf, written) } -> convertible_to<NTSTATUS>;
};

export
template<typename T>
concept has_exclusive_field = is_same_v<decltype(T::exclusive), bool>;

export
template<typename T>
concept has_get_sync_object = requires(T t) {
    { t.get_sync_object() } -> same_as<sync_object&>;
};

export
template<typename T>
concept has_last_handle_closed = requires(T t) {
    { t.last_handle_closed() };
};
