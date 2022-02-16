#pragma once

class file : public object {
public:
    file(object& ino) : object(object_type::file), ino(ino) {
        visit_obj(ino, [](auto&& o) {
            o.acquire();
        });
    }

    ~file() {
        visit_obj(ino, [](auto&& o) {
            o.release();
        });
    }

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

    size_t read(std::span<uint8_t> s);
    size_t read(std::span<uint8_t> s, uint64_t offset);
    size_t write(std::span<const uint8_t> s);
    size_t write(std::span<const uint8_t> s, uint64_t offset);
    NTSTATUS query_directory(HANDLE event, PIO_APC_ROUTINE apc_routine,
                             void* apc_context, IO_STATUS_BLOCK& io_status_block,
                             void* file_information, uint32_t length,
                             FILE_INFORMATION_CLASS file_information_class, bool return_single_entry,
                             const UNICODE_STRING* file_name, bool restart_scan);
};

static_assert(is_readable_object_without_offset<file>);
static_assert(is_readable_object_with_offset<file>);
static_assert(is_writable_object_without_offset<file>);
static_assert(is_writable_object_with_offset<file>);
