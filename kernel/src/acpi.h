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

enum class madt_type : uint8_t {
    local_apic = 0,
    io_apic = 1,
    interrupt_source_override = 2,
    nmi_source = 3,
    local_apic_nmi = 4,
    local_apic_address_override = 5,
    io_sapic = 6,
    local_sapic = 7,
    platform_interrupt_sources = 8,
    local_x2apic = 9,
    local_x2apic_nmi = 10,
    gicc = 11,
    gicd = 12,
    gic_msi_frame = 13,
    gicr = 14,
    gic_its = 15
};

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

typedef struct {
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t enabled : 1;
    uint32_t online_capable : 1;
    uint32_t reserved : 30;
} madt_local_apic;

#pragma pack(pop)

extern void* acpi_tables;
extern unsigned int acpi_count;
