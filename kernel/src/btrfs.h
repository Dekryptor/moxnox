#pragma once

#include <array>
#include "page_cache.h"
#include "rwsem.h"
#include "handle.h"

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

static const uint64_t BTRFS_COMPAT_RO_FLAGS_FREE_SPACE_TREE = 0x1;
static const uint64_t BTRFS_COMPAT_RO_FLAGS_FREE_SPACE_TREE_VALID = 0x2;

static const uint64_t BLOCK_FLAG_DATA = 0x001;
static const uint64_t BLOCK_FLAG_SYSTEM = 0x002;
static const uint64_t BLOCK_FLAG_METADATA = 0x004;
static const uint64_t BLOCK_FLAG_RAID0 = 0x008;
static const uint64_t BLOCK_FLAG_RAID1 = 0x010;
static const uint64_t BLOCK_FLAG_DUPLICATE = 0x020;
static const uint64_t BLOCK_FLAG_RAID10 = 0x040;
static const uint64_t BLOCK_FLAG_RAID5 = 0x080;
static const uint64_t BLOCK_FLAG_RAID6 = 0x100;
static const uint64_t BLOCK_FLAG_RAID1C3 = 0x200;
static const uint64_t BLOCK_FLAG_RAID1C4 = 0x400;

static const uint64_t BTRFS_ROOT_ROOT = 0x1;
static const uint64_t BTRFS_ROOT_EXTENT = 0x2;
static const uint64_t BTRFS_ROOT_CHUNK = 0x3;
static const uint64_t BTRFS_ROOT_DEVTREE = 0x4;
static const uint64_t BTRFS_ROOT_FSTREE = 0x5;
static const uint64_t BTRFS_ROOT_TREEDIR = 0x6;
static const uint64_t BTRFS_ROOT_CHECKSUM = 0x7;
static const uint64_t BTRFS_ROOT_UUID = 0x9;
static const uint64_t BTRFS_ROOT_FREE_SPACE = 0xa;
static const uint64_t BTRFS_ROOT_DATA_RELOC = 0xfffffffffffffff7;

static const uint64_t BTRFS_FIRST_FREE_OBJECTID = 0x100;
static const uint64_t BTRFS_LAST_FREE_OBJECTID = 0xffffffffffffff00;

static const uint64_t SUBVOL_ROOT_INODE = 0x100;

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

    bool operator==(const key& k) const = default;

    std::strong_ordering operator<=>(const key& k) const {
        auto cmp = obj_id <=> k.obj_id;

        if (cmp != std::strong_ordering::equal)
            return cmp;

        cmp = obj_type <=> k.obj_type;

        if (cmp != std::strong_ordering::equal)
            return cmp;

        return offset <=> k.offset;
    }
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

struct tree_header {
    std::array<uint8_t, 32> csum;
    btrfs_uuid fs_uuid;
    uint64_t address;
    uint64_t flags;
    btrfs_uuid chunk_tree_uuid;
    uint64_t generation;
    uint64_t tree_id;
    uint32_t num_items;
    uint8_t level;
} __attribute__((packed));

static_assert(sizeof(tree_header) == 101);

static const uint64_t HEADER_FLAG_WRITTEN = 1;

struct leaf_node {
    key k;
    uint32_t offset;
    uint32_t size;
} __attribute__((packed));

static_assert(sizeof(leaf_node) == 25);

struct internal_node {
    key k;
    uint64_t address;
    uint64_t generation;
} __attribute__((packed));

static_assert(sizeof(internal_node) == 33);

struct btrfs_time {
    uint64_t seconds;
    uint32_t nanoseconds;
} __attribute__((packed));

struct inode_item {
    uint64_t generation;
    uint64_t transid;
    uint64_t st_size;
    uint64_t st_blocks;
    uint64_t block_group;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t st_mode;
    uint64_t st_rdev;
    uint64_t flags;
    uint64_t sequence;
    uint8_t reserved[32];
    btrfs_time st_atime;
    btrfs_time st_ctime;
    btrfs_time st_mtime;
    btrfs_time otime;
} __attribute__((packed));

static_assert(sizeof(inode_item) == 160);

struct root_item {
    inode_item inode;
    uint64_t generation;
    uint64_t objid;
    uint64_t block_number;
    uint64_t byte_limit;
    uint64_t bytes_used;
    uint64_t last_snapshot_generation;
    uint64_t flags;
    uint32_t num_references;
    key drop_progress;
    uint8_t drop_level;
    uint8_t root_level;
    uint64_t generation2;
    btrfs_uuid uuid;
    btrfs_uuid parent_uuid;
    btrfs_uuid received_uuid;
    uint64_t ctransid;
    uint64_t otransid;
    uint64_t stransid;
    uint64_t rtransid;
    btrfs_time ctime;
    btrfs_time otime;
    btrfs_time stime;
    btrfs_time rtime;
    uint64_t reserved[8];
} __attribute__((packed));

static_assert(sizeof(root_item) == 439);

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

struct dir_item {
    key k;
    uint64_t transid;
    uint16_t m;
    uint16_t n;
    btrfs_inode_type type;
    char name[1];
} __attribute__((packed));

static_assert(offsetof(dir_item, name) == 30);

enum class btrfs_extent_type : uint8_t {
    inline_extent = 0,
    regular = 1,
    prealloc = 2
};

struct extent_data {
    uint64_t generation;
    uint64_t decoded_size;
    uint8_t compression;
    uint8_t encryption;
    uint16_t encoding;
    enum btrfs_extent_type type;
    uint8_t data[1];
} __attribute__((packed));

static_assert(offsetof(extent_data, data) == 21);

struct extent_data2 {
    uint64_t address;
    uint64_t size;
    uint64_t offset;
    uint64_t num_bytes;
};

static_assert(sizeof(extent_data2) == 32);

struct inode_ref {
    uint64_t index;
    uint16_t n;
    char name[1];
} __attribute__((packed));

static_assert(offsetof(inode_ref, name) == 10);

struct chunk_space {
    list_entry le;
    uint64_t offset;
    uint64_t length;
};

struct chunk {
    ~chunk() {
        while (!is_list_empty(space_list)) {
            auto& s = containing_record<chunk_space>(space_list.next);

            remove_entry_list(s.le);
            free(&s);
        }
    }

    list_entry le;
    uint64_t offset;
    list_entry space_list = { &space_list, &space_list };
    list_entry pending_space_list = { &pending_space_list, &pending_space_list };
    bool space_loaded = false;
    bool changed = false;
    list_entry changed_le;
    list_entry space_pending_le;
    int64_t delta;
    uint64_t last_allocation_end = 0;
    chunk_item ci; // must be last item
};

struct root {
    list_entry le;
    uint64_t id;
    root_item ri;
    std::atomic<uint64_t> next_inode_number = 0;
    bool is_dirty = false;
    list_entry dirty_le;
};

struct inode_extent {
    list_entry le;
    uint64_t offset;
    extent_data ed;
};

static const uint64_t EXTENT_ITEM_DATA = 0x001;
static const uint64_t EXTENT_ITEM_TREE_BLOCK = 0x002;
static const uint64_t EXTENT_ITEM_SHARED_BACKREFS = 0x100;

struct extent_item {
    uint64_t refcount;
    uint64_t generation;
    uint64_t flags;
};

static_assert(sizeof(extent_item) == 24);

struct tree_block_ref {
    uint64_t offset;
};

static_assert(sizeof(tree_block_ref) == 8);

struct extent_item_tbr {
    extent_item ei;
    key_type type;
    tree_block_ref tbr;
} __attribute__((packed));

static_assert(sizeof(extent_item_tbr) == 33);

struct metadata_change {
    uint64_t address;
    uint8_t level;
    uint64_t root;
    bool remove;
    list_entry le;
};

struct free_space_info {
    uint32_t count;
    uint32_t flags;
} __attribute__((packed));

static_assert(sizeof(free_space_info) == 8);

struct block_group_item {
    uint64_t used;
    uint64_t chunk_tree;
    uint64_t flags;
} __attribute__((packed));

static_assert(sizeof(block_group_item) == 24);

static const unsigned int BTRFS_MAX_LEVEL = 8;

struct btrfs_path {
    btrfs_path() {
        for (unsigned int i = 0; i < BTRFS_MAX_LEVEL; i++) {
            data[i] = nullptr;
        }
    }

    ~btrfs_path() {
        for (unsigned int i = 0; i < BTRFS_MAX_LEVEL; i++) {
            if (data[i])
                free(data[i]);
        }
    }

    uint8_t* data[BTRFS_MAX_LEVEL];
    uint32_t slots[BTRFS_MAX_LEVEL];
};

class btrfs_filesystem;

class btrfs_inode : public inode {
public:
    btrfs_inode(btrfs_filesystem& fs, uint64_t subvol, uint64_t inode, enum btrfs_inode_type inode_type);
    ~btrfs_inode();
    bool load_inode();
    void release() override;

    nt_status open_child(std::string_view name, uint32_t create_disposition,
                         uint32_t create_options,
                         std::optional<std::reference_wrapper<btrfs_inode>>& obj);
    nt_status create_child(std::string_view name,
                           uint32_t create_options,
                           std::optional<std::reference_wrapper<btrfs_inode>>& obj);
    NTSTATUS read(std::span<uint8_t> s, uint64_t offset, uint64_t& read_bytes) override;
    NTSTATUS query_directory(std::span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class,
                             uint64_t& index) override;
    size_t read_into_cache(uint64_t page_num, uint64_t num_pages) override;
    size_t flush_cache(uint64_t page_num, uint64_t num_pages) override;

    root* r;
    btrfs_filesystem& fs;
    list_entry le;
    uint64_t inode;
    inode_item ii;
    list_entry extents = { &extents, &extents };

private:
    size_t read_zeroes_into_cache(uint64_t page_num, uint64_t num_pages);
    size_t read_inline_extent_into_cache(uint64_t page_num, uint64_t num_pages,
                                         const inode_extent& ie);
    size_t read_extent_into_cache(uint64_t page_num, uint64_t num_pages,
                                  const inode_extent& ie);
    nt_status get_last_dir_index(uint64_t& ret);
};

class btrfs_filesystem : public filesystem, public cacheable {
public:
    btrfs_filesystem(volume& vol);

    ~btrfs_filesystem() override {
        free_chunk_list();
        free_root_list();
        vol->release();
    }

    nt_status read_metadata(uint64_t addr, std::span<uint8_t> data);
    nt_status open_inode(uint64_t subvol, uint64_t inode, std::optional<std::reference_wrapper<btrfs_inode>>& obj);
    nt_status open(std::string_view fn, const std::optional<std::reference_wrapper<inode>>& dir,
                   object_ref& obj_ref, uint32_t create_disposition,
                   uint32_t create_options) override;
    size_t read_into_cache(uint64_t page_num, uint64_t num_pages) override;
    size_t flush_cache(uint64_t page_num, uint64_t num_pages) override;
    nt_status remove_item(root& r, const key& k);
    nt_status remove_item2(btrfs_path& p);
    nt_status change_key(btrfs_path& p, const key& k);
    nt_status find_item(root& r, const key& k, btrfs_path& p, bool cow);
    bool move_path_back(btrfs_path& p);
    bool move_path_forward(btrfs_path& p);
    root* find_root(uint64_t root_num);
    nt_status flush(bool make_readonly) override;
    void flush_thread();

    superblock sb;
    bool valid = false;
    list_entry chunk_list = { &chunk_list, &chunk_list };
    list_entry root_list = { &root_list, &root_list };
    btrfs_inode* root_inode = nullptr;
    spinlock inode_lock;
    list_entry inode_list = { &inode_list, &inode_list };
    rwsem trans_lock;
    root* root_root;
    root* chunk_root;
    root* extent_root = nullptr;
    root* free_space_root = nullptr;
    list_entry dirty_root_list = { &dirty_root_list, &dirty_root_list };
    list_entry metadata_changes = { &metadata_changes, &metadata_changes };
    list_entry space_pending_chunks = { &space_pending_chunks, &space_pending_chunks };
    bool readonly = false;
    object_ref flush_thread_obj;

private:
    bool load_chunks();
    bool load_roots();
    void free_chunk_list();
    void free_root_list();
    bool flush_tree(const chunk& c, uint64_t addr);
    void go_readonly();
};
