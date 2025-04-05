module;

#include "kernel.h"
import objects;
#include "spinlock.h"
import rbtree;
#include "inode.h"
#include "handle.h"
#include "page_cache.h"
#include "volume.h"
#include <charconv>

export module volume;

import print;
import bsod;
import mem;
import config;
import list;
// import btrfs;
import event;

using namespace std;

export const guid null_guid = { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };

export class filesystem;

export class volume : public object, public cacheable {
public:
    volume() : object(object_type::volume) { }

    virtual ~volume() {
        if (volume_name)
            free(volume_name);
    }

    void acquire() {
        refcount++;
    }

    virtual void release() = 0;
    virtual void flush() = 0;
    virtual uint64_t dio_read(uint64_t page_num, uint64_t num_pages,
                              pfn_iterator& it) = 0;
    virtual size_t dio_write(uint64_t page_num, uint64_t num_pages,
                             pfn_iterator& it, bool fua = false) = 0;
    uint64_t read_into_cache(uint64_t page_num, uint64_t num_pages) override;
    size_t flush_cache(uint64_t page_num, uint64_t num_pages) override;

    nt_status read(span<uint8_t> s, uint64_t offset, uint64_t& read_bytes);

    void probe();
    bool check_for_partitions();

    unsigned int part_num = 0;
    unsigned int sector_size;
    uint64_t size;
    guid partition_guid = null_guid;
    guid type_guid = null_guid;
    char* volume_name = nullptr;
    size_t volume_name_len = 0;

    spinlock mount_lock;
    optional<reference_wrapper<filesystem>> filesystem;
};

static_assert(is_readable_object_with_offset<volume>);

// btrfs.cpp
extern "C++" bool btrfs_probe(volume& vol); // FIXME - avoiding circular dependency

export class diskpart : public volume {
public:
    diskpart(volume& parent, unsigned int part_num, uint64_t disk_offset, uint64_t length,
             const guid& partition_guid, const guid& type_guid);
    ~diskpart();

    void release() override {
        if (--refcount == 0)
            delete this;
    }

    void flush() override {
        parent.flush();
    }

    uint64_t dio_read(uint64_t page_num, uint64_t num_pages,
                      pfn_iterator& it) override;
    size_t dio_write(uint64_t page_num, uint64_t num_pages,
                     pfn_iterator& it, bool fua) override;
    volume& parent;
    uint64_t disk_offset;
};

static_assert(is_readable_object_with_offset<diskpart>);

export class filesystem {
public:
    filesystem(volume* vol) : vol(vol) { }
    virtual ~filesystem() { }
    virtual nt_status open(string_view fn, const optional<reference_wrapper<inode>>& dir,
                           object_ref& obj_ref, uint32_t create_disposition,
                           uint32_t create_options, unsigned int& create_result) = 0;
    virtual nt_status flush(bool make_readonly) = 0;

    list_entry le;
    volume* vol;
    atomic<unsigned int> refcount = 0;
};

void volume::probe() {
    bool has_parts = false;

    if (part_num == 0)
        has_parts = check_for_partitions();

    // if no partitions found or is already partition, probe for filesystems
    if (!has_parts)
        btrfs_probe(*this);
}

struct part_wait {
    atomic<unsigned int> left = 0;
    object_ref event;
};

static void partition_found(void* ctx1, void* ctx2) {
    auto& part = *(diskpart*)ctx1;
    auto& pw = *(part_wait*)ctx2;

    part.probe();

    // FIXME - give name if parent has one

    part.release();

    if (--pw.left == 0)
        set_event(static_cast<event&>(pw.event.get()));
}

bool volume::check_for_partitions() {
    auto first_sects = (uint8_t*)malloc(sector_size * 2);
    if (!first_sects) {
        print("malloc failed\n");
        return false;
    }

    uint64_t read_bytes;
    auto status = read(span(first_sects, sector_size * 2), 0, read_bytes);

    if (!success(status)) {
        print("volume::read returned {}\n", status);
        free(first_sects);
        return false;
    } else if (read_bytes < sector_size * 2) {
        print("volume::read failed\n");
        free(first_sects);
        return false;
    }

    auto& mbr = *(master_boot_record*)first_sects;

    if (mbr.signature != mbr_signature) { // not an MBR or GPT
        free(first_sects);
        return false;
    }

    // FIXME - proper MBR partitions

    bool has_gpt = false;
    for (const auto& part : mbr.partitions) {
        if (part.os_indicator == gpt_protective) {
            has_gpt = true;
            break;
        }
    }

    if (!has_gpt) {
        free(first_sects);
        return false;
    }

    auto& gpt = *(efi_partition_table_header*)(first_sects + sector_size);

    // FIXME - if GPT not valid, get alternative version from end of disk

    if (gpt.signature != gpt_signature) {
        print("Invalid GPT signature\n");
        free(first_sects);
        return false;
    }

    // FIXME - check header CRC32

    if (gpt.this_lba != 1) {
        print("Invalid LBA value in GPT\n");
        free(first_sects);
        return false;
    }

    if (gpt.number_of_partition_entries == 0) {
        free(first_sects);
        return true;
    }

    if (gpt.size_of_partition_entry < 128) {
        print("partition entry size was 0x{:x}, expected at least 0x80\n", gpt.size_of_partition_entry);
        free(first_sects);
        return false;
    }

    auto partition_entry_lba = gpt.partition_entry_lba;
    auto size_of_partition_entry = gpt.size_of_partition_entry;
    auto number_of_partition_entries = gpt.number_of_partition_entries;

    // FIXME - store disk GUID

    free(first_sects);

    size_t table_start = partition_entry_lba * sector_size;
    size_t table_end = table_start + (size_of_partition_entry * number_of_partition_entries);

    size_t bufsize = table_end - table_start;

    auto buf = (uint8_t*)vmalloc(bufsize);
    if (!buf) {
        print("vmalloc failed\n");
        return false;
    }

    status = read(span(buf, table_end - table_start), table_start, read_bytes);

    if (!success(status)) {
        print("volume::read returned {}\n", status);
        vfree(buf, bufsize);
        return false;
    } else if (read_bytes < table_end - table_start) {
        print("volume::read failed\n");
        vfree(buf, bufsize);
        return false;
    }

    auto ptr = buf;

    // FIXME - check CRC32 of partition list

    part_wait pw;

    pw.event = create_event(false);

    for (unsigned int i = 0; i < number_of_partition_entries; i++) {
        auto& part = *(efi_partition_entry*)ptr;

        if (part.partition_type_guid != null_guid) {
            if (part.ending_lba <= part.starting_lba) {
                print("invalid GPT entry, skipping\n");
                ptr += size_of_partition_entry;
                continue;
            }

            pw.left++;
        }

        ptr += size_of_partition_entry;
    }

    if (pw.left == 0) {
        vfree(buf, bufsize);
        return true;
    }

    ptr = buf;

    for (unsigned int i = 0; i < number_of_partition_entries; i++) {
        auto& part = *(efi_partition_entry*)ptr;

        if (part.partition_type_guid != null_guid) {
            if (part.ending_lba <= part.starting_lba) {
                ptr += size_of_partition_entry;
                continue;
            }

            auto partdev = new diskpart(*this, i + 1, part.starting_lba * sector_size,
                                        (part.ending_lba - part.starting_lba + 1) * sector_size,
                                        part.unique_partition_guid, part.partition_type_guid);
            if (!partdev) {
                print("new failed\n");
                vfree(buf, bufsize);
                return false;
            }

            if (volume_name_len > 0) {
                char num[5];
                // add "p" if parent name ends in number
                bool add_p = volume_name[volume_name_len - 1] >= '0' && volume_name[volume_name_len - 1] <= '9';

                if (auto [ptr, ec] = to_chars(num, num + sizeof(num), i + 1); ec == errc()) {
                    partdev->volume_name_len = volume_name_len;
                    partdev->volume_name_len += (size_t)(ptr - num);

                    if (add_p)
                        partdev->volume_name_len++;

                    partdev->volume_name = (char*)malloc(partdev->volume_name_len);
                    if (!partdev->volume_name) {
                        print("malloc failed\n");
                        vfree(buf, bufsize);
                        return false;
                    }

                    char* p = partdev->volume_name;

                    memcpy(p, volume_name, volume_name_len);
                    p += volume_name_len;

                    if (add_p) {
                        *p = 'p';
                        p++;
                    }

                    memcpy(p, num, (size_t)(ptr - num));

                    auto sv = string_view(partdev->volume_name, partdev->volume_name_len);

                    if (!name_object(*partdev, sv)) {
                        print("name_object failed\n");
                        vfree(buf, bufsize);
                        return false;
                    }
                }
            }

            add_kernel_thread(partition_found, partdev, &pw);
        }

        ptr += size_of_partition_entry;
    }

    vfree(buf, bufsize);

    wait_for_object(pw.event.get(), false);

    return true;
}

nt_status volume::read(span<uint8_t> data, uint64_t offset, uint64_t& read_bytes) {
    // FIXME - clear page cache in destructor

    // FIXME - return error if offset past end of device
    // FIXME - truncate data buffer if we would read past end of device

    // FIXME - return errors properly
    read_bytes = cache.read(data, offset);
    return nt_status::SUCCESS;
}

diskpart::diskpart(volume& parent, unsigned int part_num, uint64_t disk_offset, uint64_t size, const guid& partition_guid, const guid& type_guid) : parent(parent), disk_offset(disk_offset) {
    parent.acquire();

    this->part_num = part_num;
    this->partition_guid = partition_guid;
    this->type_guid = type_guid;
    this->size = size;
}

diskpart::~diskpart() {
    parent.release();
}

#if 0
size_t diskpart::read_uncached(span<uint8_t> s, uint64_t offset) {
    if (offset >= length)
        return 0;

    if (offset + s.size() > length)
        s = s.subspan(length - offset);

    if (s.empty())
        return 0;

    offset += disk_offset;

    return parent.read_uncached(s, offset);
}
#endif

uint64_t diskpart::dio_read(uint64_t page_num, uint64_t num_pages,
                            pfn_iterator& it) {
    // FIXME - reduce num_pages if would overflow partition

    return parent.dio_read(page_num + (disk_offset >> PAGE_SHIFT), num_pages,
                           it);
}

uint64_t volume::read_into_cache(uint64_t page_num, uint64_t num_pages) {
    page_cache_iterator iter(*this, page_num);

    return dio_read(page_num, num_pages, iter);
}

size_t diskpart::dio_write(uint64_t page_num, uint64_t num_pages,
                           pfn_iterator& it, bool fua) {
    // FIXME - reduce num_pages if would overflow partition

    return parent.dio_write(page_num + (disk_offset >> PAGE_SHIFT), num_pages,
                            it, fua);
}

size_t volume::flush_cache(uint64_t page_num, uint64_t num_pages) {
    if (!cache.pin(page_num, num_pages, nullptr))
        return 0;

    page_cache_iterator iter(*this, page_num);

    auto flushed = dio_write(page_num, num_pages, iter);

    if (flushed < num_pages)
        cache.unpin(page_num + flushed, num_pages - flushed);

    if (flushed == 0)
        return 0;

    // mark page cache entries as clean

    {
        lock_guard lg(cache.cache_lock);

        auto* p = cache.find_page_cache_entry(page_num);

        for (unsigned int i = 0; i < flushed; i++) {
            if (i != 0)
                p = cache.next_page_cache_entry(*p);

            assert(p->tree.key == page_num + i);

            p->status = page_cache_status::clean;
        }
    }

    cache.unpin(page_num, flushed);

    return flushed;
}
