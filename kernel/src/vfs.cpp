module;

#include <optional>
#include <string_view>
#include <atomic>
#include <span>
#include <stdint.h>

export module vfs;

import nt;
import list;
import volume;
import object_ref;
import spinlock;
import objects;
import rwsem;
import page_cache;

using namespace std;

export enum class inode_type : uint8_t {
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

extern "C++" class filesystem;

export struct dentry;

export class inode : public object {
public:
    inode(filesystem& fs) : object(object_type::inode), fs(fs) { }

    virtual ~inode() {
    }

    void acquire() {
        refcount++;
    }

    virtual void release() = 0;

    nt_status read(span<uint8_t> s, uint64_t offset, uint64_t& read_bytes);
    nt_status write(span<const uint8_t> s, uint64_t offset,
                    uint64_t& written_bytes);

    virtual NTSTATUS query_directory(span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class,
                                     uint64_t& index) = 0;
    virtual NTSTATUS set_information(span<const uint8_t> buffer,
                                     FILE_INFORMATION_CLASS file_information_class) = 0;
    virtual NTSTATUS query_information(span<uint8_t> buffer,
                                       FILE_INFORMATION_CLASS file_information_class,
                                       uint64_t& retlen) = 0;

    virtual nt_status read_into_cache(uint64_t page_num, uint64_t num_pages,
                                      uint64_t& read_pages) = 0;
    virtual nt_status flush_cache(uint64_t page_num, uint64_t num_pages) = 0;

    filesystem& fs;
    rwsem inode_lock;
    uint64_t file_size;
    enum inode_type inode_type;
    page_cache<inode> cache{*this};
    list_entry writeback_le;
    dentry* dentry;
};

static_assert(is_readable_object_with_offset<inode>);
static_assert(is_writable_object_with_offset<inode>);
static_assert(cacheable_flushable<inode>);
static_assert(cache_supports_writeback<inode>);

export struct dentry {
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
    atomic<unsigned int> refcount = 1;
    size_t name_len;
    char name[0];
};

export extern "C++" class filesystem {
public:
    filesystem(volume* vol) : vol(vol) { }
    virtual ~filesystem() { }
    virtual nt_status open(string_view fn, inode& dir, object_ref& obj_ref,
                           uint32_t create_disposition, uint32_t create_options,
                           unsigned int& create_result) = 0;
    virtual nt_status flush(bool make_readonly) = 0;

    list_entry le;
    volume* vol;
    atomic<unsigned int> refcount = 0;
    inode* root_inode = nullptr;
    spinlock writeback_lock;
    list_entry writeback_list = { &writeback_list, &writeback_list };
};

export struct filesystem_recognizer {
    list_entry le;
    bool (*func)(volume&);
};

export class file : public object {
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

    NTSTATUS read(span<uint8_t> s, uint64_t& read_bytes);
    NTSTATUS read(span<uint8_t> s, uint64_t offset, uint64_t& read_bytes);
    NTSTATUS write(span<const uint8_t> s, uint64_t& written_bytes);
    NTSTATUS write(span<const uint8_t> s, uint64_t offset,
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

export list_entry filesystem_types = { &filesystem_types, &filesystem_types };
export dentry* root_dentry;

export void add_fs_to_list(::filesystem& fs);
export bool mount_root();
export nt_status open_inode(string_view fn, inode& dir,
                            object_ref& obj_ref, uint32_t create_disposition,
                            uint32_t create_options, unsigned int& create_result);
export nt_status flush_filesystems(bool make_readonly);
