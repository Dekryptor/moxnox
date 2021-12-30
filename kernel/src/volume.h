#pragma once

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

    void probe();
    bool check_for_partitions();

    unsigned int part_num = 0;
};

static_assert(is_readable_object_with_offset<volume>);

class filesystem {
public:
    filesystem(volume& vol) : vol(vol) { }
    virtual ~filesystem() { }
    virtual unique_handle open(std::string_view fn) = 0;

    list_entry le;
    volume& vol;
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
