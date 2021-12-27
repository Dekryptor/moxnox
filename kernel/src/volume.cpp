#include "kernel.h"
#include "objects.h"
#include "spinlock.h"

using namespace std;

static spinlock fs_lock;
static list_entry fs_list = { &fs_list, &fs_list };
static filesystem* root_fs = nullptr;

void volume::probe() {
    // FIXME - partitions (GPT and MBR)

    // if no partitions found or is already partition, probe for filesystems
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
