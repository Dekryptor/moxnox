#pragma once

#include <array>

using btrfs_uuid = std::array<uint8_t, 16>;

static const uint64_t btrfs_magic = 0x4d5f53665248425f;

static const uint64_t BTRFS_COMPAT_RO_FLAGS_FREE_SPACE_CACHE = 0x1;
static const uint64_t BTRFS_COMPAT_RO_FLAGS_FREE_SPACE_CACHE_VALID = 0x2;
static const uint64_t BTRFS_COMPAT_RO_FLAGS_VERITY = 0x4;

static const uint64_t BTRFS_INCOMPAT_FLAGS_MIXED_BACKREF = 0x0001;
static const uint64_t BTRFS_INCOMPAT_FLAGS_DEFAULT_SUBVOL = 0x0002;
static const uint64_t BTRFS_INCOMPAT_FLAGS_MIXED_GROUPS = 0x0004;
static const uint64_t BTRFS_INCOMPAT_FLAGS_COMPRESS_LZO = 0x0008;
static const uint64_t BTRFS_INCOMPAT_FLAGS_COMPRESS_ZSTD = 0x0010;
static const uint64_t BTRFS_INCOMPAT_FLAGS_BIG_METADATA = 0x0020;
static const uint64_t BTRFS_INCOMPAT_FLAGS_EXTENDED_IREF = 0x0040;
static const uint64_t BTRFS_INCOMPAT_FLAGS_RAID56 = 0x0080;
static const uint64_t BTRFS_INCOMPAT_FLAGS_SKINNY_METADATA = 0x0100;
static const uint64_t BTRFS_INCOMPAT_FLAGS_NO_HOLES = 0x0200;
static const uint64_t BTRFS_INCOMPAT_FLAGS_METADATA_UUID = 0x0400;
static const uint64_t BTRFS_INCOMPAT_FLAGS_RAID1C34 = 0x0800;

struct dev_item {
    uint64_t dev_id;
    uint64_t num_bytes;
    uint64_t bytes_used;
    uint32_t optimal_io_align;
    uint32_t optimal_io_width;
    uint32_t minimal_io_size;
    uint64_t type;
    uint64_t generation;
    uint64_t start_offset;
    uint32_t dev_group;
    uint8_t seek_speed;
    uint8_t bandwidth;
    btrfs_uuid device_uuid;
    btrfs_uuid fs_uuid;
} __attribute__((packed));

struct superblock_backup {
    uint64_t root_tree_addr;
    uint64_t root_tree_generation;
    uint64_t chunk_tree_addr;
    uint64_t chunk_tree_generation;
    uint64_t extent_tree_addr;
    uint64_t extent_tree_generation;
    uint64_t fs_tree_addr;
    uint64_t fs_tree_generation;
    uint64_t dev_root_addr;
    uint64_t dev_root_generation;
    uint64_t csum_root_addr;
    uint64_t csum_root_generation;
    uint64_t total_bytes;
    uint64_t bytes_used;
    uint64_t num_devices;
    uint64_t reserved[4];
    uint8_t root_level;
    uint8_t chunk_root_level;
    uint8_t extent_root_level;
    uint8_t fs_root_level;
    uint8_t dev_root_level;
    uint8_t csum_root_level;
    uint8_t reserved2[10];
} __attribute__((packed));

struct superblock {
    uint8_t checksum[32];
    btrfs_uuid uuid;
    uint64_t sb_phys_addr;
    uint64_t flags;
    uint64_t magic;
    uint64_t generation;
    uint64_t root_tree_addr;
    uint64_t chunk_tree_addr;
    uint64_t log_tree_addr;
    uint64_t log_root_transid;
    uint64_t total_bytes;
    uint64_t bytes_used;
    uint64_t root_dir_objectid;
    uint64_t num_devices;
    uint32_t sector_size;
    uint32_t node_size;
    uint32_t leaf_size;
    uint32_t stripe_size;
    uint32_t n;
    uint64_t chunk_root_generation;
    uint64_t compat_flags;
    uint64_t compat_ro_flags;
    uint64_t incompat_flags;
    uint16_t csum_type;
    uint8_t root_level;
    uint8_t chunk_root_level;
    uint8_t log_root_level;
    dev_item dev_item;
    std::array<char, 0x100> label;
    uint64_t cache_generation;
    uint64_t uuid_tree_generation;
    btrfs_uuid metadata_uuid;
    uint64_t reserved[28];
    std::array<uint8_t, 0x800> sys_chunk_array;
    std::array<superblock_backup, 4> backup;
    uint8_t reserved2[565];
} __attribute__((packed));

static_assert(sizeof(superblock) == 4096);

class btrfs_filesystem {
public:
    btrfs_filesystem(volume& vol);

    ~btrfs_filesystem() {
        vol.release();
    }

    volume& vol;
    superblock sb;
    bool valid = false;
};
