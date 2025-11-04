#pragma once

#include <array>
#include <atomic>

import list;

using btrfs_uuid = std::array<uint8_t, 16>;

static constexpr uint64_t BTRFS_MAGIC = 0x4d5f53665248425f;

static constexpr uint64_t BTRFS_FEATURE_INCOMPAT_MIXED_BACKREF = 1 << 0;
static constexpr uint64_t BTRFS_FEATURE_INCOMPAT_DEFAULT_SUBVOL = 1 << 1;
static constexpr uint64_t BTRFS_FEATURE_INCOMPAT_MIXED_GROUPS = 1 << 2;
static constexpr uint64_t BTRFS_FEATURE_INCOMPAT_COMPRESS_LZO = 1 << 3;
static constexpr uint64_t BTRFS_FEATURE_INCOMPAT_COMPRESS_ZSTD = 1 << 4;
static constexpr uint64_t BTRFS_FEATURE_INCOMPAT_BIG_METADATA = 1 << 5;
static constexpr uint64_t BTRFS_FEATURE_INCOMPAT_EXTENDED_IREF = 1 << 6;
static constexpr uint64_t BTRFS_FEATURE_INCOMPAT_RAID56 = 1 << 7;
static constexpr uint64_t BTRFS_FEATURE_INCOMPAT_SKINNY_METADATA = 1 << 8;
static constexpr uint64_t BTRFS_FEATURE_INCOMPAT_NO_HOLES = 1 << 9;
static constexpr uint64_t BTRFS_FEATURE_INCOMPAT_METADATA_UUID = 1 << 10;
static constexpr uint64_t BTRFS_FEATURE_INCOMPAT_RAID1C34 = 1 << 11;
static constexpr uint64_t BTRFS_FEATURE_INCOMPAT_ZONED = 1 << 12;
static constexpr uint64_t BTRFS_FEATURE_INCOMPAT_EXTENT_TREE_V2 = 1 << 13;
static constexpr uint64_t BTRFS_FEATURE_INCOMPAT_RAID_STRIPE_TREE = 1 << 14;
static constexpr uint64_t BTRFS_FEATURE_INCOMPAT_SIMPLE_QUOTA = 1 << 16;

static constexpr uint64_t BTRFS_FEATURE_COMPAT_RO_FREE_SPACE_TREE = 1 << 0;
static constexpr uint64_t BTRFS_FEATURE_COMPAT_RO_FREE_SPACE_TREE_VALID = 1 << 1;
static constexpr uint64_t BTRFS_FEATURE_COMPAT_RO_VERITY = 1 << 2;
static constexpr uint64_t BTRFS_FEATURE_COMPAT_RO_BLOCK_GROUP_TREE = 1 << 3;

static constexpr uint64_t BTRFS_BLOCK_GROUP_DATA = 1 << 0;
static constexpr uint64_t BTRFS_BLOCK_GROUP_SYSTEM = 1 << 1;
static constexpr uint64_t BTRFS_BLOCK_GROUP_METADATA = 1 << 2;
static constexpr uint64_t BTRFS_BLOCK_GROUP_RAID0 = 1 << 3;
static constexpr uint64_t BTRFS_BLOCK_GROUP_RAID1 = 1 << 4;
static constexpr uint64_t BTRFS_BLOCK_GROUP_DUP = 1 << 5;
static constexpr uint64_t BTRFS_BLOCK_GROUP_RAID10 = 1 << 6;
static constexpr uint64_t BTRFS_BLOCK_GROUP_RAID5 = 1 << 7;
static constexpr uint64_t BTRFS_BLOCK_GROUP_RAID6 = 1 << 8;
static constexpr uint64_t BTRFS_BLOCK_GROUP_RAID1C3 = 1 << 9;
static constexpr uint64_t BTRFS_BLOCK_GROUP_RAID1C4 = 1 << 10;

static constexpr uint64_t BTRFS_ROOT_TREE_OBJECTID = 0x1;
static constexpr uint64_t BTRFS_EXTENT_TREE_OBJECTID = 0x2;
static constexpr uint64_t BTRFS_CHUNK_TREE_OBJECTID = 0x3;
static constexpr uint64_t BTRFS_DEV_TREE_OBJECTID = 0x4;
static constexpr uint64_t BTRFS_FS_TREE_OBJECTID = 0x5;
static constexpr uint64_t BTRFS_ROOT_TREE_DIR_OBJECTID = 0x6;
static constexpr uint64_t BTRFS_CSUM_TREE_OBJECTID = 0x7;
static constexpr uint64_t BTRFS_UUID_TREE_OBJECTID = 0x9;
static constexpr uint64_t BTRFS_FREE_SPACE_TREE_OBJECTID = 0xa;
static constexpr uint64_t BTRFS_BLOCK_GROUP_TREE_OBJECTID = 0xb;
static constexpr uint64_t BTRFS_EXTENT_CSUM_OBJECTID = 0xfffffffffffffff6;
static constexpr uint64_t BTRFS_DATA_RELOC_TREE_OBJECTID = 0xfffffffffffffff7;

static constexpr uint64_t BTRFS_FIRST_FREE_OBJECTID = 0x100;
static constexpr uint64_t BTRFS_LAST_FREE_OBJECTID = 0xffffffffffffff00;

static constexpr uint64_t BTRFS_INODE_NODATASUM = 1 << 0;
static constexpr uint64_t BTRFS_INODE_NODATACOW = 1 << 1;
static constexpr uint64_t BTRFS_INODE_READONLY = 1 << 2;
static constexpr uint64_t BTRFS_INODE_NOCOMPRESS = 1 << 3;
static constexpr uint64_t BTRFS_INODE_PREALLOC = 1 << 4;
static constexpr uint64_t BTRFS_INODE_SYNC = 1 << 5;
static constexpr uint64_t BTRFS_INODE_IMMUTABLE = 1 << 6;
static constexpr uint64_t BTRFS_INODE_APPEND = 1 << 7;
static constexpr uint64_t BTRFS_INODE_NODUMP = 1 << 8;
static constexpr uint64_t BTRFS_INODE_NOATIME = 1 << 9;
static constexpr uint64_t BTRFS_INODE_DIRSYNC = 1 << 10;
static constexpr uint64_t BTRFS_INODE_COMPRESS = 1 << 11;
static constexpr uint64_t BTRFS_INODE_ROOT_ITEM_INIT = (uint64_t)1 << 31;

static const uint64_t SUBVOL_ROOT_INODE = 0x100;

struct btrfs_dev_item {
    uint64_t devid;
    uint64_t total_bytes;
    uint64_t bytes_used;
    uint32_t io_align;
    uint32_t io_width;
    uint32_t sector_size;
    uint64_t type;
    uint64_t generation;
    uint64_t start_offset;
    uint32_t dev_group;
    uint8_t seek_speed;
    uint8_t bandwidth;
    btrfs_uuid uuid;
    btrfs_uuid fsid;
} __attribute__((packed));

struct btrfs_root_backup {
    uint64_t tree_root;
    uint64_t tree_root_gen;
    uint64_t chunk_root;
    uint64_t chunk_root_gen;
    uint64_t extent_root;
    uint64_t extent_root_gen;
    uint64_t fs_root;
    uint64_t fs_root_gen;
    uint64_t dev_root;
    uint64_t dev_root_gen;
    uint64_t csum_root;
    uint64_t csum_root_gen;
    uint64_t total_bytes;
    uint64_t bytes_used;
    uint64_t num_devices;
    uint64_t unused_64[4];
    uint8_t tree_root_level;
    uint8_t chunk_root_level;
    uint8_t extent_root_level;
    uint8_t fs_root_level;
    uint8_t dev_root_level;
    uint8_t csum_root_level;
    uint8_t unused_8[10];
} __attribute__((packed));

struct btrfs_super_block {
    uint8_t csum[32];
    btrfs_uuid fsid;
    uint64_t bytenr;
    uint64_t flags;
    uint64_t magic;
    uint64_t generation;
    uint64_t root;
    uint64_t chunk_root;
    uint64_t log_root;
    uint64_t __unused_log_root_transid;
    uint64_t total_bytes;
    uint64_t bytes_used;
    uint64_t root_dir_objectid;
    uint64_t num_devices;
    uint32_t sectorsize;
    uint32_t nodesize;
    uint32_t __unused_leafsize;
    uint32_t stripesize;
    uint32_t sys_chunk_array_size;
    uint64_t chunk_root_generation;
    uint64_t compat_flags;
    uint64_t compat_ro_flags;
    uint64_t incompat_flags;
    uint16_t csum_type;
    uint8_t root_level;
    uint8_t chunk_root_level;
    uint8_t log_root_level;
    btrfs_dev_item dev_item;
    std::array<char, 0x100> label;
    uint64_t cache_generation;
    uint64_t uuid_tree_generation;
    btrfs_uuid metadata_uuid;
    uint64_t nr_global_roots;
    uint64_t reserved[27];
    std::array<uint8_t, 0x800> sys_chunk_array;
    std::array<btrfs_root_backup, 4> super_roots;
    uint8_t padding[565];
} __attribute__((packed));

static_assert(sizeof(btrfs_super_block) == 4096);

enum class key_type : uint8_t {
    INODE_ITEM = 0x01,
    INODE_REF = 0x0c,
    INODE_EXTREF = 0x0d,
    XATTR_ITEM = 0x18,
    VERITY_DESC_ITEM = 0x24,
    VERITY_MERKLE_ITEM = 0x25,
    ORPHAN_INODE = 0x30,
    DIR_LOG_INDEX = 0x48,
    DIR_ITEM = 0x54,
    DIR_INDEX = 0x60,
    EXTENT_DATA = 0x6c,
    EXTENT_CSUM = 0x80,
    ROOT_ITEM = 0x84,
    ROOT_BACKREF = 0x90,
    ROOT_REF = 0x9c,
    EXTENT_ITEM = 0xa8,
    METADATA_ITEM = 0xa9,
    EXTENT_OWNER_REF = 0xac,
    TREE_BLOCK_REF = 0xb0,
    EXTENT_DATA_REF = 0xb2,
    SHARED_BLOCK_REF = 0xb6,
    SHARED_DATA_REF = 0xb8,
    BLOCK_GROUP_ITEM = 0xc0,
    FREE_SPACE_INFO = 0xc6,
    FREE_SPACE_EXTENT = 0xc7,
    FREE_SPACE_BITMAP = 0xc8,
    DEV_EXTENT = 0xcc,
    DEV_ITEM = 0xd8,
    CHUNK_ITEM = 0xe4,
    RAID_STRIPE = 0xe6,
    QGROUP_STATUS = 0xf0,
    QGROUP_INFO = 0xf2,
    QGROUP_LIMIT = 0xf4,
    QGROUP_RELATION = 0xf6,
    TEMPORARY_ITEM = 0xf8,
    PERSISTENT_ITEM = 0xf9,
    DEV_REPLACE = 0xfa,
    UUID_SUBVOL = 0xfb,
    UUID_RECEIVED_SUBVOL = 0xfc,
    STRING_ITEM = 0xfd
};

struct btrfs_key {
    uint64_t objectid;
    key_type type;
    uint64_t offset;

    bool operator==(const btrfs_key& k) const = default;

    std::strong_ordering operator<=>(const btrfs_key& k) const {
        auto cmp = objectid <=> k.objectid;

        if (cmp != std::strong_ordering::equal)
            return cmp;

        cmp = type <=> k.type;

        if (cmp != std::strong_ordering::equal)
            return cmp;

        return offset <=> k.offset;
    }
} __attribute__((packed));

static_assert(sizeof(btrfs_key) == 17);

struct btrfs_stripe {
    uint64_t devid;
    uint64_t offset;
    btrfs_uuid dev_uuid;
} __attribute__((packed));

struct btrfs_chunk {
    uint64_t length;
    uint64_t owner;
    uint64_t stripe_len;
    uint64_t type;
    uint32_t io_align;
    uint32_t io_width;
    uint32_t sector_size;
    uint16_t num_stripes;
    uint16_t sub_stripes;
    btrfs_stripe stripe[1];
} __attribute__((packed));

struct btrfs_header {
    std::array<uint8_t, 32> csum;
    btrfs_uuid fsid;
    uint64_t bytenr;
    uint64_t flags;
    btrfs_uuid chunk_tree_uuid;
    uint64_t generation;
    uint64_t owner;
    uint32_t nritems;
    uint8_t level;
} __attribute__((packed));

static_assert(sizeof(btrfs_header) == 101);

static const uint64_t HEADER_FLAG_WRITTEN = 1;

struct btrfs_item {
    btrfs_key key;
    uint32_t offset;
    uint32_t size;
} __attribute__((packed));

static_assert(sizeof(btrfs_item) == 25);

struct btrfs_key_ptr {
    btrfs_key key;
    uint64_t blockptr;
    uint64_t generation;
} __attribute__((packed));

static_assert(sizeof(btrfs_key_ptr) == 33);

struct btrfs_timespec {
    uint64_t sec;
    uint32_t nsec;
} __attribute__((packed));

struct btrfs_inode_item {
    uint64_t generation;
    uint64_t transid;
    uint64_t size;
    uint64_t nbytes;
    uint64_t block_group;
    uint32_t nlink;
    uint32_t uid;
    uint32_t gid;
    uint32_t mode;
    uint64_t rdev;
    uint64_t flags;
    uint64_t sequence;
    uint64_t reserved[4];
    btrfs_timespec atime;
    btrfs_timespec ctime;
    btrfs_timespec mtime;
    btrfs_timespec otime;
} __attribute__((packed));

static_assert(sizeof(btrfs_inode_item) == 160);

struct btrfs_root_item {
    btrfs_inode_item inode;
    uint64_t generation;
    uint64_t root_dirid;
    uint64_t bytenr;
    uint64_t byte_limit;
    uint64_t bytes_used;
    uint64_t last_snapshot;
    uint64_t flags;
    uint32_t refs;
    btrfs_key drop_progress;
    uint8_t drop_level;
    uint8_t level;
    uint64_t generation_v2;
    btrfs_uuid uuid;
    btrfs_uuid parent_uuid;
    btrfs_uuid received_uuid;
    uint64_t ctransid;
    uint64_t otransid;
    uint64_t stransid;
    uint64_t rtransid;
    btrfs_timespec ctime;
    btrfs_timespec otime;
    btrfs_timespec stime;
    btrfs_timespec rtime;
    uint64_t reserved[8];
} __attribute__((packed));

static_assert(sizeof(btrfs_root_item) == 439);

enum class btrfs_inode_type : uint8_t {
    unknown = 0,
    file = 1,
    directory = 2,
    chardev = 3,
    blockdev = 4,
    fifo = 5,
    socket = 6,
    symlink = 7,
    ea = 8
};

struct btrfs_dir_item {
    btrfs_key location;
    uint64_t transid;
    uint16_t data_len;
    uint16_t name_len;
    btrfs_inode_type type;
    char name[1];
} __attribute__((packed));

static_assert(offsetof(btrfs_dir_item, name) == 30);

enum class btrfs_extent_type : uint8_t {
    inline_extent = 0,
    regular = 1,
    prealloc = 2
};

struct btrfs_file_extent_item {
    uint64_t generation;
    uint64_t ram_bytes;
    uint8_t compression;
    uint8_t encryption;
    uint16_t other_encoding;
    enum btrfs_extent_type type;
    uint64_t disk_bytenr;
    uint64_t disk_num_bytes;
    uint64_t offset;
    uint64_t num_bytes;
} __attribute__((packed));

static_assert(sizeof(btrfs_file_extent_item) == 53);

struct btrfs_inode_ref {
    uint64_t index;
    uint16_t name_len;
    char name[1];
} __attribute__((packed));

static_assert(offsetof(btrfs_inode_ref, name) == 10);

struct chunk_space {
    list_entry le;
    uint64_t offset;
    uint64_t length;
};

struct root {
    list_entry le;
    uint64_t id;
    btrfs_root_item ri;
    std::atomic<uint64_t> next_inode_number = 0;
    bool is_dirty = false;
    list_entry dirty_le;
};

struct inode_extent {
    list_entry le;
    uint64_t offset;
    btrfs_file_extent_item ed;
};

static constexpr uint64_t BTRFS_EXTENT_FLAG_DATA = 1 << 0;
static constexpr uint64_t BTRFS_EXTENT_FLAG_TREE_BLOCK = 1 << 1;
static constexpr uint64_t BTRFS_BLOCK_FLAG_FULL_BACKREF = 1 << 8;

struct btrfs_extent_item {
    uint64_t refs;
    uint64_t generation;
    uint64_t flags;
};

static_assert(sizeof(btrfs_extent_item) == 24);

struct tree_block_ref {
    uint64_t offset;
};

static_assert(sizeof(tree_block_ref) == 8);

struct shared_block_ref {
    uint64_t offset;
};

static_assert(sizeof(shared_block_ref) == 8);

struct extent_item_tbr {
    btrfs_extent_item ei;
    key_type type;
    tree_block_ref tbr;
} __attribute__((packed));

static_assert(sizeof(extent_item_tbr) == 33);

struct btrfs_extent_data_ref {
    uint64_t root;
    uint64_t objectid;
    uint64_t offset;
    uint32_t count;
} __attribute__((packed));

static_assert(sizeof(btrfs_extent_data_ref) == 28);

struct extent_item_edr {
    btrfs_extent_item ei;
    key_type type;
    btrfs_extent_data_ref edr;
} __attribute__((packed));

static_assert(sizeof(extent_item_edr) == 53);

struct btrfs_shared_data_ref {
    uint32_t count;
} __attribute__((packed));

static_assert(sizeof(btrfs_shared_data_ref) == 4);

struct btrfs_extent_owner_ref {
    uint64_t root_id;
} __attribute__((packed));

static_assert(sizeof(btrfs_extent_owner_ref) == 8);

struct btrfs_free_space_info {
    uint32_t extent_count;
    uint32_t flags;
} __attribute__((packed));

static_assert(sizeof(btrfs_free_space_info) == 8);

struct btrfs_block_group_item {
    uint64_t used;
    uint64_t chunk_objectid;
    uint64_t flags;
} __attribute__((packed));

static_assert(sizeof(btrfs_block_group_item) == 24);

static const unsigned int BTRFS_MAX_LEVEL = 8;
