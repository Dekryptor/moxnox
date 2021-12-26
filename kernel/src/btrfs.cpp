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

    if (sb.num_devices != 1) {
        print("Multiple devices not supported\n");
        return;
    }

    // FIXME - make sure sb.n < sizeof(sb.sys_chunk_array)
    auto chunk_array = span(sb.sys_chunk_array.data(), sb.n);

    while (!chunk_array.empty()) {
        if (chunk_array.size() < sizeof(key)) {
            print("Chunk array truncated\n");
            return;
        }

        auto& k = *(key*)chunk_array.data();

        if (k.obj_type != key_type::CHUNK_ITEM) {
            print("Unexpected item in chunk array\n");
            return;
        }

        print_hex(k.obj_id);
        print(",");
        print_hex((uint8_t)k.obj_type);
        print(",");
        print_hex(k.offset);
        print("\n");

        chunk_array = chunk_array.subspan(sizeof(key));

        if (chunk_array.size() < offsetof(chunk_item, stripes)) {
            print("Chunk array truncated\n");
            return;
        }

        auto& ci = *(chunk_item*)chunk_array.data();
        size_t chunk_size = offsetof(chunk_item, stripes) + (sizeof(chunk_item_stripe) * ci.num_stripes);

        if (chunk_array.size() < chunk_size) {
            print("Chunk array truncated\n");
            return;
        }

        if (ci.type & (BLOCK_FLAG_RAID0 | BLOCK_FLAG_RAID1 | BLOCK_FLAG_RAID10 | BLOCK_FLAG_RAID5 | BLOCK_FLAG_RAID6 | BLOCK_FLAG_RAID1C3 | BLOCK_FLAG_RAID1C4)) {
            print("RAID not supported\n");
            return;
        }

        auto cptr = (chunk*)malloc(offsetof(chunk, ci) + chunk_size);
        if (!cptr) {
            print("malloc failed\n");
            return;
        }

        auto& c = *cptr;

        c.offset = k.offset;
        memcpy(&c.ci, chunk_array.data(), chunk_size);
        insert_tail_list(chunk_list, c.le);

        chunk_array = chunk_array.subspan(chunk_size);
    }

    // FIXME - load chunks properly
    print("FIXME - btrfs\n");

    valid = true;
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
