#pragma once

#include <atomic>

import inode;
import list;
import objects;
import spinlock;

struct dentry {
    ~dentry() {
        if (inode)
            inode->release();
    }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount != 0)
            return;

        delete this;
    }

    inode* inode = nullptr;
    list_entry le;
    unsigned int index;
    std::atomic<unsigned int> refcount = 1;
    size_t name_len;
    char name[0];
};

class file : public object {
public:
    file(object& ino, uint32_t create_options);
    ~file();

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    object& ino;
    spinlock lock;
    uint64_t file_offset = 0;
    uint64_t query_dir_index = 0;
    char* name = nullptr;
    size_t namelen = 0;
    uint32_t create_options;

    NTSTATUS read(std::span<uint8_t> s, uint64_t& read_bytes);
    NTSTATUS read(std::span<uint8_t> s, uint64_t offset, uint64_t& read_bytes);
    NTSTATUS write(std::span<const uint8_t> s, uint64_t& written_bytes);
    NTSTATUS write(std::span<const uint8_t> s, uint64_t offset,
                   uint64_t& written_bytes);
    NTSTATUS query_directory(HANDLE event, PIO_APC_ROUTINE apc_routine,
                             void* apc_context, IO_STATUS_BLOCK& io_status_block,
                             void* file_information, uint32_t length,
                             FILE_INFORMATION_CLASS file_information_class, bool return_single_entry,
                             const UNICODE_STRING* file_name, bool restart_scan);
    NTSTATUS set_information(void* buffer, uint32_t buffer_size,
                             FILE_INFORMATION_CLASS file_information_class);
    NTSTATUS query_information(void* buffer, uint32_t buffer_size,
                               FILE_INFORMATION_CLASS file_information_class,
                               uint64_t& retlen);
};

static_assert(is_readable_object_without_offset<file>);
static_assert(is_readable_object_with_offset<file>);
static_assert(is_writable_object_without_offset<file>);
static_assert(is_writable_object_with_offset<file>);
