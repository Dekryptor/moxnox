#pragma once

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
