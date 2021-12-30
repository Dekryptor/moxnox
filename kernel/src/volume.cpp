#include "kernel.h"
#include "objects.h"
#include "spinlock.h"
#include "unique_handle.h"

using namespace std;

static spinlock fs_lock;
static list_entry fs_list = { &fs_list, &fs_list };
static filesystem* root_fs = nullptr;

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

    // FIXME - choose root FS based on command line
    root_fs = &containing_record<filesystem>(fs_list.next);
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

bool volume::check_for_partitions() {
    auto first_sect = (uint8_t*)malloc(4096);
    if (!first_sect) {
        print("malloc failed\n");
        return false;
    }

    if (!read(span(first_sect, 4096), 0)) {
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

    print("FIXME - scan for GPT partitions\n");

    // FIXME - GPT

    free(first_sect);

    return false;
}
