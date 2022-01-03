#pragma once

class inode : public object {
public:
    inode() : object(object_type::inode) { }

    virtual ~inode() {
    }

    void acquire() {
        refcount++;
    }

    virtual void release() = 0;

    virtual size_t read(std::span<uint8_t> s, uint64_t offset) = 0;
    uint64_t file_size;
};

static_assert(is_readable_object_with_offset<inode>);
