#include "kernel.h"
#include "acpi.h"

using namespace std;

static span<const uint8_t> find_mcfg() {
    optional<reference_wrapper<const ACPISDTHeader>> mcfgw;

    find_acpi_table(ACPI_MCFG, [&](const ACPISDTHeader& tbl) {
        if (mcfgw)
            return;

        mcfgw = tbl;
    });

    if (!mcfgw) {
        print("MCFG not found in ACPI tables.\n");
        return span<const uint8_t>();
    }

    auto& mcfg = mcfgw->get();

    if (mcfg.Length < offsetof(apic_mcfg, items)) {
        print("MCFG was shorter than expected.\n");
        return span<const uint8_t>();
    }

    return span((const uint8_t*)&mcfg, mcfg.Length);
}

void init_pci() {
    auto mcfg = find_mcfg();

    if (mcfg.empty())
        return;

    const auto& tbl = *(apic_mcfg*)mcfg.data();

    auto items = span(tbl.items, (mcfg.size() - offsetof(apic_mcfg, items)) / sizeof(apic_mcfg_item));

    for (const auto& i : items) {
        print("base_address = ");
        print_hex(i.base_address);
        print(", segment_group_number = ");
        print_hex(i.segment_group_number);
        print(", start_number = ");
        print_hex(i.start_number);
        print(", end_number = ");
        print_hex(i.end_number);
        print("\n");
    }

    // FIXME
}
