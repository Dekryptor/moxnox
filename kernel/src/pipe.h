#pragma once

#include "spinlock.h"
#include <span>
#include <string_view>

class pipe : public object {
public:
    pipe(unsigned int buffer_pages);
    virtual ~pipe();

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    virtual NTSTATUS read(std::span<uint8_t> s, uint64_t& read_bytes);

    virtual size_t write(std::span<const uint8_t> s);

    size_t write(std::string_view sv) {
        return write(std::span((uint8_t*)sv.data(), sv.length()));
    }

    virtual nt_status ioctl(uint32_t, std::span<const uint8_t>, std::span<uint8_t>, uintptr_t&) {
        return nt_status::NOT_SUPPORTED;
    }

    virtual nt_status flush(bool go_readonly) {
        if (go_readonly)
            readonly = true;

        return nt_status::SUCCESS;
    }

    bool signalled = false;
    uint8_t* buf;
    unsigned int buf_pages;
    size_t buf_unread = 0;
    size_t buf_write_pos = 0;
    spinlock lock;
    event not_full_event;
    bool readonly = false;
    list_entry flushable_le;
};

static_assert(is_sync_object<pipe>);
static_assert(is_readable_object_without_offset<pipe>);
static_assert(is_writable_object_without_offset<pipe>);
static_assert(has_ioctls<pipe>);

// pipe.cpp
void add_to_flushable_pipes_list(pipe& p);
void shutdown_pipes();
