module;

#include <span>
#include <optional>
#include <atomic>
#include <stddef.h>
#include <stdint.h>

export module volume;

import nt;
import list;
import guid;
import page_cache;
import rwsem;
import objects;
import spinlock;
import mem;

using namespace std;

export const guid null_guid = { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };

export extern "C++" class filesystem;

export class volume : public object {
public:
    volume() : object(object_type::volume) { }

    virtual ~volume() {
        if (volume_name)
            free(volume_name);
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    virtual nt_status flush() = 0;
    virtual nt_status dio_read(uint64_t page_num, uint64_t num_pages,
                               pfn_iterator& it, uint64_t& read_pages) = 0;
    virtual nt_status dio_write(uint64_t page_num, uint64_t num_pages,
                                pfn_iterator& it, uint64_t& written_pages,
                                bool fua = false) = 0;

    nt_status read(span<uint8_t> s, uint64_t offset, uint64_t& read_bytes);
    nt_status read_into_cache(uint64_t page_num, uint64_t num_pages,
                              uint64_t& read_pages);
    nt_status flush_cache(uint64_t page_num, uint64_t num_pages);

    void probe();
    bool check_for_partitions();

    unsigned int part_num = 0;
    unsigned int sector_size;
    uint64_t size;
    guid partition_guid = null_guid;
    guid type_guid = null_guid;
    char* volume_name = nullptr;
    size_t volume_name_len = 0;
    page_cache<volume> cache{*this};
    spinlock part_lock;
    list_entry part_list = { &part_list, &part_list };

    rwsem mount_lock;
    optional<reference_wrapper<::filesystem>> filesystem;
};

static_assert(cacheable_flushable<volume>);

static_assert(is_readable_object_with_offset<volume>);

export class diskpart : public volume {
public:
    diskpart(volume& parent, unsigned int part_num, uint64_t disk_offset, uint64_t length,
             const guid& partition_guid, const guid& type_guid);
    ~diskpart();

    nt_status flush() override {
        return parent.flush();
    }

    nt_status dio_read(uint64_t page_num, uint64_t num_pages,
                       pfn_iterator& it, uint64_t& read_pages) override;
    nt_status dio_write(uint64_t page_num, uint64_t num_pages,
                        pfn_iterator& it, uint64_t& written_pages,
                        bool fua) override;
    volume& parent;
    uint64_t disk_offset_pages;
    uint64_t length_pages;
    list_entry part_le;
};

static_assert(is_readable_object_with_offset<diskpart>);
