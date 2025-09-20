module;

#include <charconv>
#include <nt.h>
#include <span>
#include <optional>
#include <functional>
#include <atomic>
#include <string_view>
#include <mutex>
#include "spinlock.h"
import rwsem;
#include "inode.h"
#include "handle.h"
#include "scheduler.h"
#include "assert.h"

export module volume;

import print;
import bsod;
import mem;
import config;
import list;
import event;
import thread;
import guid;

#include "volume.h"

using namespace std;

export const guid null_guid = { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };

export extern "C++" class filesystem;
export class volume;

export class volume : public object {
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
    virtual nt_status flush() = 0;
    virtual nt_status dio_read(uint64_t page_num, uint64_t num_pages,
                               pfn_iterator& it, uint64_t& read_pages) = 0;
    virtual nt_status dio_write(uint64_t page_num, uint64_t num_pages,
                                pfn_iterator& it, uint64_t& written_pages,
                                bool fua = false) = 0;

    nt_status read(span<uint8_t> s, uint64_t offset, uint64_t& read_bytes);
    uint64_t read_into_cache(uint64_t page_num, uint64_t num_pages);
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

    rwsem mount_lock;
    optional<reference_wrapper<::filesystem>> filesystem;
};

static_assert(cacheable_flushable<volume>);

static_assert(is_readable_object_with_offset<volume>);

class diskpart : public volume {
public:
    diskpart(volume& parent, unsigned int part_num, uint64_t disk_offset, uint64_t length,
             const guid& partition_guid, const guid& type_guid);
    ~diskpart();

    void release() override {
        if (--refcount == 0)
            delete this;
    }

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
};

static_assert(is_readable_object_with_offset<diskpart>);

void volume::probe() {
    if (part_num == 0)
        check_for_partitions();
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

            uint64_t start_address = part.starting_lba * (uint64_t)sector_size;
            uint64_t part_length = (part.ending_lba - part.starting_lba + 1) * (uint64_t)sector_size;

            if (start_address % PAGE_SIZE) {
                print("skipping partition which doesn't start on page size boundary\n");
                ptr += size_of_partition_entry;
                continue;
            }

            if (part_length % PAGE_SIZE)
                part_length -= part_length % PAGE_SIZE;

            auto partdev = new diskpart(*this, i + 1, start_address, part_length,
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

                    status = name_object(*partdev, sv);
                    if (!success(status)) {
                        print("name_object returned {}\n", status);
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

diskpart::diskpart(volume& parent, unsigned int part_num, uint64_t disk_offset, uint64_t size, const guid& partition_guid, const guid& type_guid) : parent(parent) {
    parent.acquire();

    disk_offset_pages = disk_offset >> PAGE_SHIFT;

    this->part_num = part_num;
    this->partition_guid = partition_guid;
    this->type_guid = type_guid;
    this->size = size;
}

diskpart::~diskpart() {
    parent.release();
}

nt_status diskpart::dio_read(uint64_t page_num, uint64_t num_pages,
                             pfn_iterator& it, uint64_t& read_pages) {
    if (page_num >= disk_offset_pages + (size >> PAGE_SHIFT))
        return nt_status::INVALID_PARAMETER;

    num_pages = min(num_pages, disk_offset_pages + (size >> PAGE_SHIFT) - page_num);

    return parent.dio_read(page_num + disk_offset_pages, num_pages,
                           it, read_pages);
}

uint64_t volume::read_into_cache(uint64_t page_num, uint64_t num_pages) {
    page_cache_entry* p;

    for (uint64_t i = 0; i < num_pages; i++) {
        lock_guard lg(cache.cache_lock);

        if (i == 0)
            p = cache.find_page_cache_entry(page_num);
        else
            p = cache.next_page_cache_entry(*p);

        if (p->pfn == 0) {
            auto pa = allocate_folio(0);
            if (!pa) {
                print("allocate_folio failed\n");
                return 0;
            }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            p->pfn = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
        }
    }

    page_cache_iterator iter(cache, page_num);

    uint64_t read_pages;

    auto status = dio_read(page_num, num_pages, iter, read_pages);
    if (!success(status)) {
        print("dio_read returned {}\n", status);
        return 0;
    }

    return read_pages;
}

nt_status diskpart::dio_write(uint64_t page_num, uint64_t num_pages,
                              pfn_iterator& it, uint64_t& written_pages,
                              bool fua) {
    if (page_num >= disk_offset_pages + (size >> PAGE_SHIFT))
        return nt_status::INVALID_PARAMETER;

    num_pages = min(num_pages, disk_offset_pages + (size >> PAGE_SHIFT) - page_num);

    return parent.dio_write(page_num + disk_offset_pages, num_pages,
                            it, written_pages, fua);
}

nt_status volume::flush_cache(uint64_t page_num, uint64_t num_pages) {
    if (!cache.pin(page_num, num_pages, nullptr))
        return nt_status::INTERNAL_ERROR;

    page_cache_iterator iter(cache, page_num);

    while (num_pages > 0) {
        uint64_t flushed;

        auto status = dio_write(page_num, num_pages, iter, flushed);
        if (!success(status)) {
            cache.unpin(page_num, num_pages);
            return status;
        }

        if (flushed == 0) {
            cache.unpin(page_num, num_pages);
            return nt_status::INTERNAL_ERROR;
        }

        // mark page cache entries as clean

        {
            lock_guard lg(cache.cache_lock);

            auto* p = cache.find_page_cache_entry(page_num);

            for (unsigned int i = 0; i < flushed; i++) {
                if (i != 0)
                    p = cache.next_page_cache_entry(*p);

                assert(p->tree.key == page_num + i);

                p->dirty = 0;
            }
        }

        cache.unpin(page_num, flushed);

        ++iter;
        page_num += flushed;
        num_pages -= flushed;
    }

    return nt_status::SUCCESS;
}
