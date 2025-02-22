#include "kernel.h"
#include "objects.h"
#include "spinlock.h"
#include "handle.h"

import format;

using namespace std;

static const uint64_t read_sector_size = 0x1000;

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
    auto first_sect = (uint8_t*)malloc(read_sector_size);
    if (!first_sect) {
        print("malloc failed\n");
        return false;
    }

    uint64_t read_bytes;
    auto status = read(span(first_sect, read_sector_size), 0, read_bytes);

    if (!success(status)) {
        print("volume::read returned {}\n", status);
        free(first_sect);
        return false;
    } else if (read_bytes < read_sector_size) {
        print("volume::read failed\n");
        free(first_sect);
        return false;
    }

    auto& mbr = *(master_boot_record*)first_sect;

    if (mbr.signature != mbr_signature) { // not an MBR or GPT
        free(first_sect);
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
        free(first_sect);
        return false;
    }

    auto& gpt = *(efi_partition_table_header*)(first_sect + 0x200);

    // FIXME - if GPT not valid, get alternative version from end of disk

    if (gpt.signature != gpt_signature) {
        print("Invalid GPT signature\n");
        free(first_sect);
        return false;
    }

    // FIXME - check header CRC32

    if (gpt.this_lba != 1) {
        print("Invalid LBA value in GPT\n");
        free(first_sect);
        return false;
    }

    if (gpt.number_of_partition_entries == 0) {
        free(first_sect);
        return true;
    }

    if (gpt.size_of_partition_entry < 128) {
        print("partition entry size was 0x{:x}, expected at least 0x80\n", gpt.size_of_partition_entry);
        free(first_sect);
        return false;
    }

    auto partition_entry_lba = gpt.partition_entry_lba;
    auto size_of_partition_entry = gpt.size_of_partition_entry;
    auto number_of_partition_entries = gpt.number_of_partition_entries;

    // FIXME - store disk GUID

    free(first_sect);

    size_t table_start = partition_entry_lba * sector_size;
    size_t table_end = table_start + (size_of_partition_entry * number_of_partition_entries);
    size_t table_start_aligned = table_start & ~(read_sector_size - 1);
    size_t table_end_aligned = table_end & ~(read_sector_size - 1) ? ((table_end & ~(read_sector_size - 1)) + read_sector_size) : table_end;

    auto buf = (uint8_t*)malloc(table_end_aligned - table_start_aligned);
    if (!buf) {
        print("malloc failed\n");
        return false;
    }

    status = read(span(buf, table_end_aligned - table_start_aligned), table_start_aligned, read_bytes);

    if (!success(status)) {
        print("volume::read returned {}\n", status);
        free(buf);
        return false;
    } else if (read_bytes < table_end_aligned - table_start_aligned) {
        print("volume::read failed\n");
        free(buf);
        return false;
    }

    auto ptr = buf + table_start - table_start_aligned;

    // FIXME - check CRC32 of partition list

    auto pw = new part_wait;
    if (!pw) {
        print("new failed\n");
        free(buf);
        return false;
    }

    pw->event = create_event(false);

    for (unsigned int i = 0; i < number_of_partition_entries; i++) {
        auto& part = *(efi_partition_entry*)ptr;

        if (part.partition_type_guid != null_guid) {
            if (part.ending_lba <= part.starting_lba) {
                print("invalid GPT entry, skipping\n");
                ptr += size_of_partition_entry;
                continue;
            }

            pw->left++;
        }

        ptr += size_of_partition_entry;
    }

    if (pw->left == 0) {
        delete pw;
        free(buf);
        return true;
    }

    ptr = buf + table_start - table_start_aligned;

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
                free(buf);
                return false;
            }

            if (volume_name_len > 0) {
                char num[5];
                // add "p" if parent name ends in number
                bool add_p = volume_name[volume_name_len - 1] >= '0' && volume_name[volume_name_len - 1] <= '9';

                if (auto [ptr, ec] = to_chars(num, num + sizeof(num), i + 1); ec == errc()) {
                    partdev->volume_name_len = volume_name_len;
                    partdev->volume_name_len += ptr - num;

                    if (add_p)
                        partdev->volume_name_len++;

                    partdev->volume_name = (char*)malloc(partdev->volume_name_len);
                    if (!partdev->volume_name) {
                        print("malloc failed\n");
                        free(buf);
                        return false;
                    }

                    char* p = partdev->volume_name;

                    memcpy(p, volume_name, volume_name_len);
                    p += volume_name_len;

                    if (add_p) {
                        *p = 'p';
                        p++;
                    }

                    memcpy(p, num, ptr - num);

                    auto sv = string_view(partdev->volume_name, partdev->volume_name_len);

                    if (!name_object(*partdev, sv, true)) {
                        print("name_object failed\n");
                        free(buf);
                        return false;
                    }
                }
            }

            add_kernel_thread(partition_found, partdev, pw);
        }

        ptr += size_of_partition_entry;
    }

    free(buf);

    wait_for_object(pw->event.get(), false);

    delete pw;

    return true;
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

NTSTATUS diskpart::read(span<uint8_t> data, uint64_t offset, uint64_t& read_bytes) {
    // FIXME - clear page cache in destructor

    // FIXME - return error if offset past end of device
    // FIXME - truncate data buffer if we would read past end of device

    // FIXME - return errors properly
    read_bytes = cache.read(data, offset);
    return nt_status::SUCCESS;
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

uint64_t diskpart::read_into_cache(uint64_t page_num, uint64_t num_pages) {
    // FIXME - make sure that partition is aligned to page boundaries

    auto read_pages = parent.cache.read_into_cache_and_pin(page_num + (disk_offset >> PAGE_SHIFT), num_pages);

    if (read_pages == 0)
        return 0;

    auto* src = parent.cache.find_page_cache_entry(page_num + (disk_offset >> PAGE_SHIFT));
    auto* dst = cache.find_page_cache_entry(page_num);

    for (uint64_t i = 0; i < read_pages; i++) {
        // FIXME - steal pages?
        memcpy(phys_address(dst->phys_address), phys_address(src->phys_address),
               PAGE_SIZE);
        dst->status = page_cache_status::clean;

        if (i != read_pages - 1) {
            src = &containing_record<page_cache_entry>(src->le.next);
            dst = &containing_record<page_cache_entry>(dst->le.next);
        }
    }

    parent.cache.unpin(page_num + (disk_offset >> PAGE_SHIFT), read_pages);

    return read_pages;
}

size_t diskpart::flush_cache(uint64_t page_num, uint64_t num_pages) {
    // FIXME - need lock? Others shouldn't be writing into bits of the page cache we're flushing

    if (!cache.pin(page_num, num_pages, nullptr))
        return 0;

    uint64_t flushed = 0;
    auto ent = cache.find_page_cache_entry(page_num);

    for (uint64_t i = 0; i < num_pages; i++) {
        assert(ent->page_num == page_num + i);

        if (ent->status == page_cache_status::dirty) {
            auto dirty_page = span((uint8_t*)phys_address(ent->phys_address), PAGE_SIZE);

            if (!parent.cache.write_pages(page_num + (disk_offset >> PAGE_SHIFT) + i,
                                          1, dirty_page)) {
                break;
            }

            ent->status = page_cache_status::clean;
        }

        if (i != num_pages - 1)
            ent = &containing_record<page_cache_entry>(ent->le.next);

        flushed++;
    }

    cache.unpin(page_num, num_pages);

    if (flushed == 0)
        return 0;

    uint64_t flushed2 = 0;
    uint64_t cur_page = page_num + (disk_offset >> PAGE_SHIFT);

    while (true) {
        auto done = parent.flush_cache(cur_page, flushed);

        if (done == 0)
            break;

        flushed2 += min(done, flushed);

        if (done >= flushed)
            break;

        cur_page += done;
        flushed -= done;
    }

    return flushed2;
}
