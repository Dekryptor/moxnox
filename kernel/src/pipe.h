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

    NTSTATUS read(std::span<uint8_t> s, uint64_t& read_bytes);

    virtual size_t write(std::span<const uint8_t> s);

    size_t write(std::string_view sv) {
        return write(std::span((uint8_t*)sv.data(), sv.length()));
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
static_assert(is_writable_object_without_offset<pipe>);

class duplex_pipe : public object {
public:
    duplex_pipe(pipe& recv_pipe, pipe& send_pipe) : object(object_type::duplex_pipe), recv_pipe(recv_pipe), send_pipe(send_pipe) {
        recv_pipe.acquire();
        send_pipe.acquire();
    }

    ~duplex_pipe() {
        recv_pipe.release();
        send_pipe.release();
    }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    NTSTATUS read(std::span<uint8_t> s, uint64_t& read_bytes) {
        return recv_pipe.read(s, read_bytes);
    }

    size_t write(std::span<const uint8_t> s) {
        return send_pipe.write(s);
    }

    pipe& recv_pipe;
    pipe& send_pipe;
};

static_assert(is_readable_object_without_offset<duplex_pipe>);
static_assert(is_writable_object_without_offset<duplex_pipe>);
