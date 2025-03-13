#pragma once

extern void* acpi_tables;
extern unsigned int acpi_count;

static const uint32_t ACPI_APIC = 0x43495041;
static const uint32_t ACPI_MCFG = 0x4746434d;

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
