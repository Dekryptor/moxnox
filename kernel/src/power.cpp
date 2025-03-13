#include "kernel.h"
#include "acpica.h"
#include "acpi.h"
#include "objects.h"
#include "pipe.h"

import print;
import inout;

using namespace std;

static void reboot() {
    optional<reference_wrapper<const ACPI_TABLE_HEADER>> fadtw;

    __asm __volatile("cli");

    find_acpi_table(ACPI_FACP, [&](const ACPI_TABLE_HEADER& tbl) {
        if (fadtw)
            return;

        fadtw = tbl;
    });

    if (!fadtw) {
        print("FADT not found in ACPI tables.\n");
        return;
    }

    auto& fadt = reinterpret_cast<const ACPI_TABLE_FADT&>(fadtw->get());

    if (fadt.Header.Length < offsetofend(ACPI_TABLE_FADT, ResetValue)) {
        print("FADT was truncated\n");
        return;
    }

    if (fadt.ResetRegister.BitWidth != 8) {
        print("invalid value for ResetRegister.BitWidth in FADT\n");
        return;
    }

    if (fadt.ResetRegister.BitOffset != 0) {
        print("invalid value for ResetRegister.BitOffset in FADT\n");
        return;
    }

    switch (fadt.ResetRegister.SpaceId) {
        case ACPI_ADR_SPACE_SYSTEM_MEMORY:
            print("FIXME - ACPI_ADR_SPACE_SYSTEM_MEMORY\n");
        break;

        case ACPI_ADR_SPACE_SYSTEM_IO:
            outb((uint16_t)fadt.ResetRegister.Address, fadt.ResetValue);
        break;

        case ACPI_ADR_SPACE_PCI_CONFIG:
            print("FIXME - ACPI_ADR_SPACE_PCI_CONFIG\n");
        break;

        default:
            print("invalid value {} for ResetRegister.SpaceId in FADT\n",
                  (unsigned int)fadt.ResetRegister.SpaceId);
        break;
    }
}

static void halt() {
    AcpiEnterSleepStatePrep(5);

    __asm("cli");

    AcpiEnterSleepState(5);
}

NTSTATUS NtShutdownSystem(enum SHUTDOWN_ACTION action) {
    if (action == SHUTDOWN_ACTION::ShutdownNoReboot) {
        // FIXME
        return nt_status::NOT_IMPLEMENTED;
    }

    if (action != SHUTDOWN_ACTION::ShutdownPowerOff && action != SHUTDOWN_ACTION::ShutdownReboot)
        return nt_status::INVALID_PARAMETER;

    // flush filesystems and make readonly
    auto status = flush_filesystems(true);
    if (!success(status))
        return status;

    __asm __volatile("cli");

    if (num_cpus > 1) {
        processors_left_to_halt = num_cpus - 1;

        // send IPI to other cores to stop them

        auto icr1 = (volatile uint32_t*)&local_apic[0x300];

        *icr1 = 0xc0000 | halt_interrupt;

        while (processors_left_to_halt > 0) { }
    }

    disable_preempt();

    __asm __volatile("sti");

    print("shutting down\n");
    shutdown_pipes();

    if (action == SHUTDOWN_ACTION::ShutdownPowerOff)
        halt();
    else
        reboot();

    enable_preempt();

    return nt_status::INTERNAL_ERROR;
}
