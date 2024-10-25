#include "kernel.h"
#include "acpi.h"

using namespace std;

static const uint16_t SLP_EN = 1 << 13;

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

static void halt() {
    // FIXME - call _PTS ACPI method ("Prepare to Sleep")

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

    if (fadt.h.Length < offsetofend(struct fadt, X_PM1aControlBlock)) {
        printk("FADT was truncated\n");
        return;
    }

    if (fadt.X_PM1aControlBlock.AddressSpace != acpi_address_space::system_io) {
        printk("unexpected value for X_PM1aControlBlock.AddressSpace in FADT\n");
        return;
    }

    if (fadt.X_PM1aControlBlock.BitWidth != 16) {
        printk("unexpected value for X_PM1aControlBlock.BitWidth in FADT\n");
        return;
    }

    if (fadt.X_PM1aControlBlock.BitOffset != 0) {
        printk("unexpected value for X_PM1aControlBlock.BitOffset in FADT\n");
        return;
    }

    // FIXME - parse DSDT AML to find what _S5 SLP_TYP should actually be
    static const uint16_t SLP_TYP5 = 0;

    outw((uint16_t)fadt.X_PM1aControlBlock.Address, SLP_EN | SLP_TYP5);
}

NTSTATUS NtShutdownSystem(enum SHUTDOWN_ACTION action) {
    // FIXME - flush and unmount filesystems, close running programs, etc.

    switch (action) {
        case SHUTDOWN_ACTION::ShutdownPowerOff:
            halt();
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
