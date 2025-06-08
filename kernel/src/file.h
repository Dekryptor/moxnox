#pragma once

class file : public object {
public:
    file(object& ino);
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
