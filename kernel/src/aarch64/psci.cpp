module;

#include <span>
#include "../acpica.h"

export module psci;

import acpi;
import print;

using namespace std;

static const uint32_t PSCI_VERSION_FUNC_ID = 0x84000000;

static uint32_t psci_version;

template<bool hvc, uint32_t id>
uint32_t psci_func() {
    uint32_t ret;

    if constexpr (hvc) {
        __asm __volatile (
            "mov w0, #%w[func]\n"
            "hvc #0\n"
            "mov %w[res], w0\n"
            : [res] "=r" (ret)
            : [func] "i" (id)
            : "x0"
        );
    } else {
        __asm __volatile (
            "mov w0, #%w[func]\n"
            "smc #0\n"
            "mov %w[res], w0\n"
            : [res] "=r" (ret)
            : [func] "i" (id)
            : "x0"
        );
    }

    return ret;
}

template<bool hvc>
uint32_t PSCI_VERSION() {
    return psci_func<hvc, PSCI_VERSION_FUNC_ID>();
}

export bool init_psci() {
    ACPI_TABLE_HEADER* tbl;

    // FIXME - don't show error if not in ACPI mode (check devicetree instead)

    auto status = AcpiGetTable((char*)ACPI_SIG_FADT, 1, &tbl);
    if (ACPI_FAILURE(status)) {
        print("AcpiGetTable returned {} for MADT\n", status);
        return false;
    }

    if (tbl->Length < sizeof(ACPI_TABLE_FADT)) {
        print("FADT table was {:x} bytes, expected at least {:x}\n",
              tbl->Length, sizeof(ACPI_TABLE_FADT));
        return false;
    }

    auto& fadt = *(ACPI_TABLE_FADT*)tbl;

    if (!(fadt.ArmBootFlags & ACPI_FADT_PSCI_COMPLIANT))
        return true;

    bool hvc = fadt.ArmBootFlags & ACPI_FADT_PSCI_USE_HVC;

    if (hvc)
        psci_version = PSCI_VERSION<true>();
    else
        psci_version = PSCI_VERSION<false>();

    print("using psci {}.{}\n", psci_version >> 16, psci_version & 0xffff);

    return true;
}
