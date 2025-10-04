module;

#include <span>
#include "../acpica.h"

export module psci;

import acpi;
import print;

using namespace std;

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

    // FIXME - ACPI_FADT_PSCI_USE_HVC

    print("FIXME - check PSCI\n");

    return true;
}
