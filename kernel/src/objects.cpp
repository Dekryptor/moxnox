module;

#include <nt.h>
#include <atomic>
#include <span>

export module objects;

import list;

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
template<typename T>
concept is_sync_object = is_same_v<decltype(T::signalled), bool> &&
    is_same_v<decltype(T::waiters), list_entry>;

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
