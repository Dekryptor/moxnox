// SPDX-License-Identifier: MIT
module;

#include <stdint.h>

export module smbios;

export struct smbios2_entry_point {
    char anchor_string[4];
    uint8_t entry_point_checksum;
    uint8_t entry_point_length;
    uint8_t major_version;
    uint8_t minor_version;
    uint16_t maximum_structure_size;
    uint8_t entry_point_revision;
    uint8_t formatted_area[5];
    char intermediate_anchor_string[5];
    uint8_t intermediate_checksum;
    uint16_t structure_table_length;
    uint32_t structure_table_address;
    uint16_t number_of_structures;
    uint8_t smbios_bcd_revision;
} __attribute__((packed));

static_assert(sizeof(smbios2_entry_point) == 0x1f);

export struct smbios3_entry_point {
    char anchor_string[5];
    uint8_t entry_point_checksum;
    uint8_t entry_point_length;
    uint8_t major_version;
    uint8_t minor_version;
    uint8_t docrev;
    uint8_t entry_point_revision;
    uint8_t reserved;
    uint32_t structure_table_maximum_size;
    uint64_t structure_table_address;
} __attribute__((packed));

static_assert(sizeof(smbios3_entry_point) == 0x18);

export enum class smbios_type : uint8_t {
    firmware_information = 0,
    system_information = 1,
    system_enclosure = 3,
    processor_information = 4,
    cache_information = 7,
    system_slots = 9,
    physical_memory_array = 16,
    memory_device = 17,
    memory_array_mapped_address = 19,
    system_boot_information = 32,
    end = 127
};

export struct smbios_header {
    smbios_type type;
    uint8_t length;
    uint16_t handle;
} __attribute__((packed));

static_assert(sizeof(smbios_header) == 0x4);

export struct smbios_system_information : smbios_header {
    uint8_t manufacturer;
    uint8_t product_name;
    uint8_t version;
    uint8_t serial_number;
    uint8_t uuid[16];
    uint8_t wakeup_type;
    uint8_t sku_number;
    uint8_t family;
} __attribute__((packed));

static_assert(sizeof(smbios_system_information) == 0x1b);
