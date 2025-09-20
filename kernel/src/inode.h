#pragma once

#include <nt.h>

import list;
import objects;
import page_cache;
import rwsem;

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

class filesystem;

class inode : public object {
public:
    inode(filesystem& fs) : object(object_type::inode), fs(fs) { }

    virtual ~inode() {
    }

    void acquire() {
        refcount++;
    }

    virtual void release() = 0;

    nt_status read(std::span<uint8_t> s, uint64_t offset, uint64_t& read_bytes);
    nt_status write(std::span<const uint8_t> s, uint64_t offset,
                    uint64_t& written_bytes);

    virtual NTSTATUS query_directory(std::span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class,
                                     uint64_t& index) = 0;
    virtual NTSTATUS set_information(std::span<const uint8_t> buffer,
                                     FILE_INFORMATION_CLASS file_information_class) = 0;
    virtual NTSTATUS query_information(std::span<uint8_t> buffer,
                                       FILE_INFORMATION_CLASS file_information_class,
                                       uint64_t& retlen) = 0;

    virtual size_t read_into_cache(uint64_t page_num, uint64_t num_pages) = 0;
    virtual nt_status flush_cache(uint64_t page_num, uint64_t num_pages) = 0;

    filesystem& fs;
    rwsem inode_lock;
    uint64_t file_size;
    enum inode_type inode_type;
    page_cache<inode> cache{*this};
    list_entry writeback_le;
};

static_assert(is_readable_object_with_offset<inode>);
static_assert(is_writable_object_with_offset<inode>);
static_assert(cacheable_flushable<inode>);
static_assert(cache_supports_writeback<inode>);
