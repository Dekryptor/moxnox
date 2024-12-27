#pragma once

class filesystem;

class volume : public object {
public:
    volume() : object(object_type::volume) { }

    virtual ~volume() {
    }

    void acquire() {
        refcount++;
    }

    virtual void release() = 0;

    virtual size_t read(std::span<uint8_t> s, uint64_t offset) = 0;
    virtual uint64_t read_into_cache(uint64_t page_num, uint64_t num_pages) = 0;
    virtual uint64_t read_into_cache_and_pin(uint64_t page_num,
                                             uint64_t num_pages) = 0;
    virtual void unpin(uint64_t page_num, uint64_t num_pages) = 0;
    virtual page_cache_entry* find_page_cache_entry(uint64_t page_num) = 0;

    void probe();
    bool check_for_partitions();

    unsigned int part_num = 0;
    unsigned int sector_size;
    guid partition_guid = null_guid;
    guid type_guid = null_guid;

    spinlock mount_lock;
    std::optional<std::reference_wrapper<filesystem>> filesystem;
};

static_assert(is_readable_object_with_offset<volume>);
static_assert(cacheable<volume>);

class diskpart : public volume {
public:
    diskpart(volume& parent, unsigned int part_num, uint64_t disk_offset, uint64_t length,
             const guid& partition_guid, const guid& type_guid);
    ~diskpart();

    void release() override {
        if (--refcount == 0)
            delete this;
    }

    size_t read(std::span<uint8_t> s, uint64_t offset) override;
    uint64_t read_into_cache(uint64_t page_num, uint64_t num_pages) override;
    uint64_t read_into_cache_and_pin(uint64_t page_num, uint64_t num_pages) override;
    void unpin(uint64_t page_num, uint64_t num_pages) override;
    page_cache_entry* find_page_cache_entry(uint64_t page_num) override;
    volume& parent;
    uint64_t disk_offset;
    uint64_t length;
};

static_assert(is_readable_object_with_offset<diskpart>);
static_assert(cacheable<diskpart>);

class filesystem {
public:
    filesystem(volume& vol) : vol(vol) { }
    virtual ~filesystem() { }
    virtual object_ref open(std::string_view fn, const std::optional<std::reference_wrapper<inode>>& dir) = 0;

    list_entry le;
    volume& vol;
    std::atomic<unsigned int> refcount = 0;
};

struct mbr_partition_record {
    uint8_t boot_indicator;
    uint8_t start_head;
    uint8_t start_sector;
    uint8_t start_track;
    uint8_t os_indicator;
    uint8_t end_head;
    uint8_t end_sector;
    uint8_t end_track;
    uint32_t starting_lba;
    uint32_t size_in_lba;
} __attribute__((packed));

struct master_boot_record {
    uint8_t bootstrap_code[440];
    uint32_t unique_mbr_signature;
    uint8_t unknown[2];
    mbr_partition_record partitions[4];
    uint16_t signature;
} __attribute__((packed));

static_assert(sizeof(master_boot_record) == 512);

static const uint16_t mbr_signature = 0xaa55;
static const uint8_t gpt_protective = 0xee;

struct efi_partition_table_header {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
    uint64_t this_lba;
    uint64_t alternative_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    guid disk_guid;
    uint64_t partition_entry_lba;
    uint32_t number_of_partition_entries;
    uint32_t size_of_partition_entry;
    uint32_t partition_entry_array_crc32;
} __attribute__((packed));

static_assert(sizeof(efi_partition_table_header) == 92);

static const uint64_t gpt_signature = 0x5452415020494645; // "EFI PART"

struct efi_partition_entry {
    guid partition_type_guid;
    guid unique_partition_guid;
    uint64_t starting_lba;
    uint64_t ending_lba;
    uint64_t attributes;
    char16_t partition_name[36];
} __attribute__((packed));

static_assert(sizeof(efi_partition_entry) == 128);
