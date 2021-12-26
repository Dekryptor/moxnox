#pragma once

#include "spinlock.h"
#include <span>
#include <string_view>

class pipe : public object {
public:
    pipe(unsigned int buffer_pages);
    ~pipe();

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    size_t read(std::span<uint8_t> s);

    void write(std::span<const uint8_t> s);

    void write(std::string_view sv) {
        write(std::span((uint8_t*)sv.data(), sv.length()));
    }

    bool signalled = false;
    uint8_t* buf;
    unsigned int buf_pages;
    size_t buf_unread = 0;
    size_t buf_write_pos = 0;
    spinlock lock;
};

static_assert(is_sync_object<pipe>);
static_assert(is_readable_object_without_offset<pipe>);
static_assert(is_writable_object<pipe>);
