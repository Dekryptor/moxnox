#include "kernel.h"
#include "objects.h"
#include "btrfs.h"

using namespace std;

static const uint64_t superblock_addrs[] = { 0x10000, 0x4000000, 0x4000000000, 0x4000000000000, 0 };

static const uint64_t supported_incompat_flags = BTRFS_INCOMPAT_FLAGS_MIXED_BACKREF | BTRFS_INCOMPAT_FLAGS_MIXED_GROUPS |
                                                 BTRFS_INCOMPAT_FLAGS_BIG_METADATA | BTRFS_INCOMPAT_FLAGS_EXTENDED_IREF |
                                                 BTRFS_INCOMPAT_FLAGS_SKINNY_METADATA | BTRFS_INCOMPAT_FLAGS_NO_HOLES;

btrfs_filesystem::btrfs_filesystem(volume& vol) : vol(vol) {
    vol.acquire();

    if (!vol.read(span((uint8_t*)&sb, sizeof(superblock)), superblock_addrs[0]))
        return;

    if (sb.magic != btrfs_magic)
        return;

    // FIXME - check checksum
    // FIXME - check other superblock addresses, and choose one with highest generation

    if (sb.incompat_flags & ~supported_incompat_flags) {
        print("Unsupported incompat flags\n");
        return;
    }

    print("FIXME - check superblock\n");

    // FIXME - load chunks
}

bool btrfs_probe(volume& vol) {
    auto fs = new btrfs_filesystem(vol);

    if (!fs->valid) {
        delete fs;
        return false;
    }

    // FIXME - add fs to list

    return true;
}
