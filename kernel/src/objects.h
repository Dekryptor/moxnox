#pragma once

enum class object_type {
    thread,
    timer,
    event,
    pipe,
    semaphore,
    volume,
    inode,
    process,
    section,
    file
};

class object {
public:
    object(enum object_type type) : type(type) { }

    enum object_type type;
    std::atomic<unsigned int> refcount = 1;
    std::atomic<unsigned int> handle_count = 0;
    bool named = false;
    bool persistent = false;
};

template<typename T>
concept is_sync_object = std::is_same_v<decltype(T::signalled), bool>;

template<typename T>
concept object_has_on_wake = requires(T t) {
    { t.on_wake() };
};

template<typename T>
concept is_writable_object_with_offset = requires(T t, std::span<const uint8_t> s, uint64_t offset, uint64_t& written_bytes) {
    { t.write(s, offset, written_bytes) } -> std::convertible_to<NTSTATUS>;
};

template<typename T>
concept is_writable_object_without_offset = requires(T t, std::span<const uint8_t> s, uint64_t& written_bytes) {
    { t.write(s, written_bytes) } -> std::convertible_to<NTSTATUS>;
};

// FIXME - version of this specifying offset
// FIXME - async reads and writes (specifying event)
template<typename T>
concept is_readable_object_with_offset = requires(T t, std::span<uint8_t> s, uint64_t offset, uint64_t& read_bytes) {
    { t.read(s, offset, read_bytes) } -> std::convertible_to<NTSTATUS>;
};

template<typename T>
concept is_readable_object_without_offset = requires(T t, std::span<uint8_t> s, uint64_t& read_bytes) {
    { t.read(s, read_bytes) } -> std::convertible_to<NTSTATUS>;
};

template<typename T>
concept has_ioctls = requires(T t, uint32_t code, std::span<const uint8_t> inbuf, std::span<uint8_t> outbuf, uintptr_t& written) {
    { t.ioctl(code, inbuf, outbuf, written) } -> std::convertible_to<NTSTATUS>;
};

template<typename T>
concept has_exclusive_field = std::is_same_v<decltype(T::exclusive), bool>;
