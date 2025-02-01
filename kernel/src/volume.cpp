#include "kernel.h"
#include "objects.h"
#include "spinlock.h"
#include "handle.h"

using namespace std;

static spinlock fs_lock;
static list_entry fs_list = { &fs_list, &fs_list };
static filesystem* root_fs = nullptr;

static const uint64_t read_sector_size = 0x1000;

static const guid moxnox_partition_guid = { 0x5dd71063, 0x95c9, 0x41b2, { 0x82, 0x52, 0x84, 0x13, 0xfe, 0xac, 0x64, 0x90 } };

void volume::probe() {
    bool has_parts = false;

    if (part_num == 0)
        has_parts = check_for_partitions();

    // if no partitions found or is already partition, probe for filesystems
    if (!has_parts)
        btrfs_probe(*this);
}

void add_fs_to_list(filesystem& fs) {
    lock_guard lock(fs_lock);
    insert_tail_list(fs_list, fs.le);
}

bool mount_root() {
    string_view cmdline_root;

    loop_through_cmdline([&](string_view s) {
        static const char root_option[] = "root=";

        if (s.size() < sizeof(root_option) - 1)
            return true;

        if (string_view(s.data(), sizeof(root_option) - 1) != root_option)
            return true;

        cmdline_root = string_view(s.data() + sizeof(root_option) - 1,
                                   s.size() - sizeof(root_option) + 1);

        return false;
    });

    if (!cmdline_root.empty()) {
        static const char prefix[] = "/dev/";

        if (cmdline_root.size() <= sizeof(prefix) - 1 ||
            string_view(cmdline_root.data(), sizeof(prefix) - 1) != prefix) {
            char s[255];
            auto sp = span(s, sizeof(s));

            sp = sp_strcpy(sp, "malformed root option ");
            sp = sp_strcpy(sp, cmdline_root);
            bsod(string_view(s, sp.data() - s));
        }

        cmdline_root = string_view(cmdline_root.data() + sizeof(prefix) - 1,
                                   cmdline_root.size() - sizeof(prefix) + 1);

        auto ref = open_object(cmdline_root);
        if (!ref) {
            char s[255];
            auto sp = span(s, sizeof(s));

            sp = sp_strcpy(sp, "could not find root volume /dev/");
            sp = sp_strcpy(sp, cmdline_root);
            bsod(string_view(s, sp.data() - s));
        }

        if (ref.get().type != object_type::volume) {
            char s[255];
            auto sp = span(s, sizeof(s));

            sp = sp_strcpy(sp, "root volume /dev/");
            sp = sp_strcpy(sp, cmdline_root);
            sp = sp_strcpy(sp, " is not a block device");
            bsod(string_view(s, sp.data() - s));
        }

        auto& vol = static_cast<volume&>(ref.get());

        {
            lock_guard lock(vol.mount_lock);

            if (!vol.filesystem) {
                char s[255];
                auto sp = span(s, sizeof(s));

                sp = sp_strcpy(sp, "no recognized filesystem found on /dev/");
                sp = sp_strcpy(sp, cmdline_root);
                bsod(string_view(s, sp.data() - s));
            }

            root_fs = &vol.filesystem->get();
            root_fs->refcount++;

            return true;
        }
    }

    // search for MN partition if no option

    lock_guard lock(fs_lock);

    if (is_list_empty(fs_list)) {
        printk("No filesytems found.\n");
        return false;
    }

    // choose MN partition

    auto le = fs_list.next;
    while (le != &fs_list) {
        auto& fs = containing_record<filesystem>(le);

        if (fs.vol.type_guid == moxnox_partition_guid) {
            if (root_fs) {
                printk("Multiple Moxnox partitions, not sure which to choose\n");
                root_fs = nullptr;
                return false;
            }

            root_fs = &fs;
        }

        le = le->next;
    }

    if (!root_fs) {
        printk("No Moxnox partition found\n");
        return false;
    }

    return true;
}

nt_status open_inode(string_view fn, const optional<reference_wrapper<inode>>& dir,
                     object_ref& obj_ref, uint32_t create_disposition,
                     uint32_t create_options) {
    if (dir && fn.empty()) {
        dir->get().acquire();
        obj_ref = dir->get();
        return nt_status::SUCCESS;
    }

    if (!root_fs) {
        printk("No root filesystem.\n");
        return nt_status::INTERNAL_ERROR;
    }

    // FIXME - handle mount points
    // FIXME - /dev

    return root_fs->open(fn, dir, obj_ref, create_disposition,
                         create_options);
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
        printk("malloc failed\n");
        return false;
    }

    uint64_t read_bytes;
    auto status = read(span(first_sect, read_sector_size), 0, read_bytes);

    if (!success(status)) {
        printk("volume::read returned ");
        print_hex((uint64_t)status, 8);
        printk("\n");
        free(first_sect);
        return false;
    } else if (read_bytes < read_sector_size) {
        printk("volume::read failed\n");
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
        printk("Invalid GPT signature\n");
        free(first_sect);
        return false;
    }

    // FIXME - check header CRC32

    if (gpt.this_lba != 1) {
        printk("Invalid LBA value in GPT\n");
        free(first_sect);
        return false;
    }

    if (gpt.number_of_partition_entries == 0) {
        free(first_sect);
        return true;
    }

    if (gpt.size_of_partition_entry < 128) {
        printk("partition entry size was 0x");
        print_hex(gpt.size_of_partition_entry);
        printk(", expected at least 0x80\n");
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
        printk("malloc failed\n");
        return false;
    }

    status = read(span(buf, table_end_aligned - table_start_aligned), table_start_aligned, read_bytes);

    if (!success(status)) {
        printk("volume::read returned ");
        print_hex((uint64_t)status, 8);
        printk("\n");
        free(buf);
        return false;
    } else if (read_bytes < table_end_aligned - table_start_aligned) {
        printk("volume::read failed\n");
        free(buf);
        return false;
    }

    auto ptr = buf + table_start - table_start_aligned;

    // FIXME - check CRC32 of partition list

    auto pw = new part_wait;
    if (!pw) {
        printk("new failed\n");
        free(buf);
        return false;
    }

    pw->event = create_event(false);

    for (unsigned int i = 0; i < number_of_partition_entries; i++) {
        auto& part = *(efi_partition_entry*)ptr;

        if (part.partition_type_guid != null_guid) {
            if (part.ending_lba <= part.starting_lba) {
                printk("invalid GPT entry, skipping\n");
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
                printk("new failed\n");
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
                        printk("malloc failed\n");
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
                        printk("name_object failed\n");
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

diskpart::diskpart(volume& parent, unsigned int part_num, uint64_t disk_offset, uint64_t length, const guid& partition_guid, const guid& type_guid) : parent(parent), disk_offset(disk_offset), length(length) {
    parent.acquire();

    this->part_num = part_num;
    this->partition_guid = partition_guid;
    this->type_guid = type_guid;
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
    printk("FIXME - diskpart::flush_cache\n"); // FIXME

    return 0;
}
