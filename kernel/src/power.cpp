#include "kernel.h"
#include "acpi.h"

using namespace std;

__attribute((naked))
static void outb(uint16_t port, uint8_t val) {
    __asm __volatile(
        "mov al, %1\n"
        "\tmov dx, %0\n"
        "\tout dx, al\n"
        "\tret\n"
        :
        : "r" (port), "r" (val)
        : "al", "dx"
    );
}

__attribute((naked))
static void outw(uint16_t port, uint16_t val) {
    __asm __volatile(
        "mov ax, %1\n"
        "\tmov dx, %0\n"
        "\tout dx, ax\n"
        "\tret\n"
        :
        : "r" (port), "r" (val)
        : "al", "dx"
    );
}

static void reboot() {
    optional<reference_wrapper<const ACPISDTHeader>> fadtw;

    find_acpi_table(ACPI_FACP, [&](const ACPISDTHeader& tbl) {
        if (fadtw)
            return;

        fadtw = tbl;
    });

    if (!fadtw) {
        printk("FADT not found in ACPI tables.\n");
        return;
    }

    auto& fadt = reinterpret_cast<const struct fadt&>(fadtw->get());

    if (fadt.h.Length < offsetofend(struct fadt, ResetValue)) {
        printk("FADT was truncated\n");
        return;
    }

    if (fadt.ResetReg.BitWidth != 8) {
        printk("invalid value for ResetReg.BitWidth in FADT\n");
        return;
    }

    if (fadt.ResetReg.BitOffset != 0) {
        printk("invalid value for ResetReg.BitOffset in FADT\n");
        return;
    }

    switch (fadt.ResetReg.AddressSpace) {
        case acpi_address_space::system_memory:
            printk("FIXME - acpi_address_space::system_memory\n");
        break;

        case acpi_address_space::system_io:
            outb((uint16_t)fadt.ResetReg.Address, fadt.ResetValue);
        break;

        case acpi_address_space::pci_config:
            printk("FIXME - acpi_address_space::pci_config\n");
        break;

        default:
            printk("invalid value for ResetReg.AddressSpace in FADT\n");
        break;
    }
}

NTSTATUS NtShutdownSystem(enum SHUTDOWN_ACTION action) {
    // FIXME - parse ACPI AML to find what constants should be

    // FIXME - flush and unmount filesystems, close running programs, etc.
    // FIXME - call _PTS ACPI method ("Prepare to Sleep")

    switch (action) {
        case SHUTDOWN_ACTION::ShutdownPowerOff:
            outw(0x604, 0x2000); // FIXME - qemu-specific
            break;

        case SHUTDOWN_ACTION::ShutdownReboot:
            reboot();
            break;

        case SHUTDOWN_ACTION::ShutdownNoReboot:
            // FIXME
            return nt_status::NOT_IMPLEMENTED;
    }

    return nt_status::INTERNAL_ERROR;
}
