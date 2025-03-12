#pragma once

#pragma pack(push,1)

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

static const uint32_t ACPI_APIC = 0x43495041;
static const uint32_t ACPI_MCFG = 0x4746434d;
static const uint32_t ACPI_FACP = 0x50434146;

static void find_acpi_table(uint32_t sig, std::invocable<const ACPI_TABLE_HEADER&> auto func) {
    auto ptr = (uint8_t*)acpi_tables;

    for (unsigned int i = 0; i < acpi_count; i++) {
        auto& tbl = *(ACPI_TABLE_HEADER*)ptr;

        // FIXME - check checksums

        if (*(uint32_t*)&tbl.Signature == sig)
            func(tbl);

        ptr += tbl.Length;
    }
}
