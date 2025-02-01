#pragma once
#include "page_cache.h"

class filesystem;

class volume : public object, public cacheable {
public:
    volume() : object(object_type::volume) { }

    virtual ~volume() {
        if (volume_name)
            free(volume_name);
    }

    void acquire() {
        refcount++;
    }

    virtual void release() = 0;

    virtual NTSTATUS read(std::span<uint8_t> s, uint64_t offset, uint64_t& read_bytes) = 0;

    void probe();
    bool check_for_partitions();

    unsigned int part_num = 0;
    unsigned int sector_size;
    guid partition_guid = null_guid;
    guid type_guid = null_guid;
    char* volume_name = nullptr;
    size_t volume_name_len = 0;

    spinlock mount_lock;
    std::optional<std::reference_wrapper<filesystem>> filesystem;
};

static_assert(is_readable_object_with_offset<volume>);

class diskpart : public volume {
public:
    diskpart(volume& parent, unsigned int part_num, uint64_t disk_offset, uint64_t length,
             const guid& partition_guid, const guid& type_guid);
    ~diskpart();

    void release() override {
        if (--refcount == 0)
            delete this;
    }

    NTSTATUS read(std::span<uint8_t> data, uint64_t offset, uint64_t& read_bytes) override;
    uint64_t read_into_cache(uint64_t page_num, uint64_t num_pages) override;
    volume& parent;
    uint64_t disk_offset;
    uint64_t length;
};

static_assert(is_readable_object_with_offset<diskpart>);

class filesystem {
public:
    filesystem(volume& vol) : vol(vol) { }
    virtual ~filesystem() { }
    virtual nt_status open(std::string_view fn, const std::optional<std::reference_wrapper<inode>>& dir,
                           object_ref& obj_ref, uint32_t create_disposition,
                           uint32_t create_options) = 0;

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
