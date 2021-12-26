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

static const uint64_t BLOCK_FLAG_RAID0 = 0x008;
static const uint64_t BLOCK_FLAG_RAID1 = 0x010;
static const uint64_t BLOCK_FLAG_DUPLICATE = 0x020;
static const uint64_t BLOCK_FLAG_RAID10 = 0x040;
static const uint64_t BLOCK_FLAG_RAID5 = 0x080;
static const uint64_t BLOCK_FLAG_RAID6 = 0x100;
static const uint64_t BLOCK_FLAG_RAID1C3 = 0x200;
static const uint64_t BLOCK_FLAG_RAID1C4 = 0x400;

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

enum class key_type : uint8_t {
    INODE_ITEM = 0x01,
    INODE_REF = 0x0c,
    INODE_EXTREF = 0x0d,
    XATTR_ITEM = 0x18,
    ORPHAN_INODE = 0x30,
    DIR_ITEM = 0x54,
    DIR_INDEX = 0x60,
    EXTENT_DATA = 0x6c,
    EXTENT_CSUM = 0x80,
    ROOT_ITEM = 0x84,
    ROOT_BACKREF = 0x90,
    ROOT_REF = 0x9c,
    EXTENT_ITEM = 0xa8,
    METADATA_ITEM = 0xa9,
    TREE_BLOCK_REF = 0xb0,
    EXTENT_DATA_REF = 0xb2,
    EXTENT_REF_V0 = 0xb4,
    SHARED_BLOCK_REF = 0xb6,
    SHARED_DATA_REF = 0xb8,
    BLOCK_GROUP_ITEM = 0xc0,
    FREE_SPACE_INFO = 0xc6,
    FREE_SPACE_EXTENT = 0xc7,
    FREE_SPACE_BITMAP = 0xc8,
    DEV_EXTENT = 0xcc,
    DEV_ITEM = 0xd8,
    CHUNK_ITEM = 0xe4,
    TEMP_ITEM = 0xf8,
    DEV_STATS = 0xf9,
    SUBVOL_UUID = 0xfb,
    SUBVOL_REC_UUID = 0xfc,
};

struct key {
    uint64_t obj_id;
    key_type obj_type;
    uint64_t offset;
} __attribute__((packed));

static_assert(sizeof(key) == 17);

struct chunk_item_stripe {
    uint64_t dev_id;
    uint64_t offset;
    btrfs_uuid dev_uuid;
} __attribute__((packed));

struct chunk_item {
    uint64_t size;
    uint64_t root_id;
    uint64_t stripe_length;
    uint64_t type;
    uint32_t opt_io_alignment;
    uint32_t opt_io_width;
    uint32_t sector_size;
    uint16_t num_stripes;
    uint16_t sub_stripes;
    chunk_item_stripe stripes[1];
} __attribute__((packed));

struct chunk {
    uint64_t offset;
    list_entry le;
    chunk_item ci;
};

class btrfs_filesystem {
public:
    btrfs_filesystem(volume& vol);

    ~btrfs_filesystem() {
        vol.release();
    }

    volume& vol;
    superblock sb;
    bool valid = false;
    list_entry chunk_list = { &chunk_list, &chunk_list };
};
