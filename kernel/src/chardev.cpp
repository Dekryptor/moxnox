module;

#include "kernel.h"
import objects;

export module chardev;

using namespace std;

export class chardev : public object {
public:
    chardev() : object(object_type::chardev) { }
    virtual ~chardev() { }

    virtual NTSTATUS read(std::span<uint8_t> s, uint64_t& read_bytes) = 0;

    virtual NTSTATUS write(std::span<const uint8_t> s, uint64_t& written_bytes) = 0;

    NTSTATUS write(std::string_view sv, uint64_t& written_bytes) {
        return write(std::span((uint8_t*)sv.data(), sv.length()), written_bytes);
    }

    virtual nt_status ioctl(uint32_t, std::span<const uint8_t>, std::span<uint8_t>, uintptr_t&) {
        return nt_status::NOT_SUPPORTED;
    }

    virtual nt_status flush(bool go_readonly) {
        if (go_readonly)
            readonly = true;

        return nt_status::SUCCESS;
    }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    bool signalled = true;
    bool exclusive = true;
    list_entry waiters = { &waiters, &waiters };
    bool readonly = false;
};

static_assert(is_sync_object<chardev>);
static_assert(is_readable_object_without_offset<chardev>);
static_assert(is_writable_object_without_offset<chardev>);
static_assert(has_ioctls<chardev>);
static_assert(has_exclusive_field<chardev>);
