#include "kernel.h"
#include "objects.h"
#include "spinlock.h"
#include "unique_handle.h"

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

void mount_root() {
    lock_guard lock(fs_lock);

    if (is_list_empty(fs_list)) {
        print("No filesytems found.\n");
        return;
    }

    // FIXME - choosing root FS based on command line

    // choose MN partition

    auto le = fs_list.next;
    while (le != &fs_list) {
        auto& fs = containing_record<filesystem>(le);

        if (fs.vol.type_guid == moxnox_partition_guid) {
            if (root_fs) {
                print("Multiple Moxnox partitions, not sure which to choose\n");
                root_fs = nullptr;
                return;
            }

            root_fs = &fs;
        }

        le = le->next;
    }

    if (!root_fs)
        print("No Moxnox partition found\n");
}

unique_handle open_file(string_view fn) {
    if (fn.empty() || fn[0] != '/') {
        print("Not an absolute path.\n");
        return {};
    }

    if (!root_fs) {
        print("No root filesystem.\n");
        return {};
    }

    fn.remove_prefix(1);

    // FIXME - handle mount points
    // FIXME - /dev

    return root_fs->open(fn);
}

struct part_wait {
    atomic<unsigned int> left = 0;
    unique_handle event;
};

static void partition_found(void* ctx1, void* ctx2) {
    auto& part = *(diskpart*)ctx1;
    auto& pw = *(part_wait*)ctx2;

    part.probe();

    // FIXME - give name if parent has one

    part.release();

    if (--pw.left == 0)
        set_event(pw.event.get());
}

bool volume::check_for_partitions() {
    auto first_sect = (uint8_t*)malloc(read_sector_size);
    if (!first_sect) {
        print("malloc failed\n");
        return false;
    }

    if (read(span(first_sect, read_sector_size), 0) < read_sector_size) {
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
        print("partition entry size was 0x");
        print_hex(gpt.size_of_partition_entry);
        print(", expected at least 0x80\n");
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

    if (read(span(buf, table_end_aligned - table_start_aligned), table_start_aligned) < table_end_aligned - table_start_aligned) {
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

            add_kernel_thread(partition_found, partdev, pw);
        }

        ptr += size_of_partition_entry;
    }

    free(buf);

    wait_for_object(pw->event.get());

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

size_t diskpart::read(span<uint8_t> s, uint64_t offset) {
    if (offset >= length)
        return 0;

    if (offset + s.size() > length)
        s = s.subspan(length - offset);

    if (s.empty())
        return 0;

    offset += disk_offset;

    return parent.read(s, offset);
}
