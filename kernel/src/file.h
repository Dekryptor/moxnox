#pragma once

class file : public object {
public:
    file(inode& ino) : object(object_type::file), ino(ino) {
        ino.acquire();
    }

    ~file() {
        ino.release();
    }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    inode& ino;
    spinlock lock;
    uint64_t file_offset = 0;

    size_t read(std::span<uint8_t> s);
    size_t read(std::span<uint8_t> s, uint64_t offset);
};

static_assert(is_readable_object_without_offset<file>);
static_assert(is_readable_object_with_offset<file>);
