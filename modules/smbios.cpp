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
