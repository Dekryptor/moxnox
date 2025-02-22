#include "kernel.h"
#include "acpi.h"

import format;
import inout;

using namespace std;

static const uint16_t SLP_EN = 1 << 13;

static void reboot() {
    optional<reference_wrapper<const ACPISDTHeader>> fadtw;

    find_acpi_table(ACPI_FACP, [&](const ACPISDTHeader& tbl) {
        if (fadtw)
            return;

        fadtw = tbl;
    });

    if (!fadtw) {
        print("FADT not found in ACPI tables.\n");
        return;
    }

    auto& fadt = reinterpret_cast<const struct fadt&>(fadtw->get());

    if (fadt.h.Length < offsetofend(struct fadt, ResetValue)) {
        print("FADT was truncated\n");
        return;
    }

    if (fadt.ResetReg.BitWidth != 8) {
        print("invalid value for ResetReg.BitWidth in FADT\n");
        return;
    }

    if (fadt.ResetReg.BitOffset != 0) {
        print("invalid value for ResetReg.BitOffset in FADT\n");
        return;
    }

    switch (fadt.ResetReg.AddressSpace) {
        case acpi_address_space::system_memory:
            print("FIXME - acpi_address_space::system_memory\n");
        break;

        case acpi_address_space::system_io:
            outb((uint16_t)fadt.ResetReg.Address, fadt.ResetValue);
        break;

        case acpi_address_space::pci_config:
            print("FIXME - acpi_address_space::pci_config\n");
        break;

        default:
            print("invalid value {} for ResetReg.AddressSpace in FADT\n",
                  (unsigned int)fadt.ResetReg.AddressSpace);
        break;
    }
}

static void halt() {
    // FIXME - call _PTS ACPI method ("Prepare to Sleep")

    optional<reference_wrapper<const ACPISDTHeader>> fadtw;

    find_acpi_table(ACPI_FACP, [&](const ACPISDTHeader& tbl) {
        if (fadtw)
            return;

        fadtw = tbl;
    });

    if (!fadtw) {
        print("FADT not found in ACPI tables.\n");
        return;
    }

    auto& fadt = reinterpret_cast<const struct fadt&>(fadtw->get());

    if (fadt.h.Length < offsetofend(struct fadt, X_PM1aControlBlock)) {
        print("FADT was truncated\n");
        return;
    }

    if (fadt.X_PM1aControlBlock.AddressSpace != acpi_address_space::system_io) {
        print("unexpected value {} for X_PM1aControlBlock.AddressSpace in FADT\n",
              (unsigned int)fadt.X_PM1aControlBlock.AddressSpace);
        return;
    }

    if (fadt.X_PM1aControlBlock.BitWidth != 16) {
        print("unexpected value {} for X_PM1aControlBlock.BitWidth in FADT\n",
              fadt.X_PM1aControlBlock.BitWidth);
        return;
    }

    if (fadt.X_PM1aControlBlock.BitOffset != 0) {
        print("unexpected value {} for X_PM1aControlBlock.BitOffset in FADT\n",
              fadt.X_PM1aControlBlock.BitOffset);
        return;
    }

    // FIXME - parse DSDT AML to find what _S5 SLP_TYP should actually be
    static const uint16_t SLP_TYP5 = 0;

    outw((uint16_t)fadt.X_PM1aControlBlock.Address, SLP_EN | SLP_TYP5);
}

NTSTATUS NtShutdownSystem(enum SHUTDOWN_ACTION action) {
    if (action == SHUTDOWN_ACTION::ShutdownNoReboot) {
        // FIXME
        return nt_status::NOT_IMPLEMENTED;
    }

    if (action != SHUTDOWN_ACTION::ShutdownPowerOff && action != SHUTDOWN_ACTION::ShutdownReboot)
        return nt_status::INVALID_PARAMETER;

    // FIXME - flush serial ports, close running programs, etc.

    auto status = flush_filesystems();
    if (!success(status))
        return status;

    if (action == SHUTDOWN_ACTION::ShutdownPowerOff)
        halt();
    else
        reboot();

    return nt_status::INTERNAL_ERROR;
}
