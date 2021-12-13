#pragma once

#pragma pack(push,1)

typedef struct {
    uint32_t Signature;
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} ACPISDTHeader;

typedef struct {
    uint8_t id;
    uint8_t reserved;
    uint32_t address;
    uint32_t interrupt_base;
} madt_ioapic;

typedef struct {
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t gsi;
    uint16_t flags;
} madt_source_override;

#pragma pack(pop)

extern void* acpi_tables;
extern unsigned int acpi_count;
