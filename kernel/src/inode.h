#pragma once

#include "page_cache.h"

enum class inode_type : uint8_t {
    unknown = 0,
    file = 1,
    directory = 2,
    chardev = 3,
    blockdev = 4,
    fifo = 5,
    socket = 6,
    symlink = 7,
    ea = 8
};

class inode : public object {
public:
    inode() : object(object_type::inode) { }

    virtual ~inode() {
    }

    void acquire() {
        refcount++;
    }

    virtual void release() = 0;

    virtual NTSTATUS read(std::span<uint8_t> s, uint64_t offset, uint64_t& read_bytes) = 0;
    virtual NTSTATUS query_directory(std::span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class,
                                     uint64_t& index) = 0;
    virtual size_t read_into_cache(uint64_t page_num, uint64_t num_pages) = 0;
    virtual size_t read_into_cache_and_pin(uint64_t page_num, uint64_t num_pages) = 0;
    virtual page_cache_entry* find_page_cache_entry(uint64_t page_num) = 0;

    uint64_t file_size;
    page_cache<inode> cache{*this};
    enum inode_type inode_type;
};

static_assert(is_readable_object_with_offset<inode>);
