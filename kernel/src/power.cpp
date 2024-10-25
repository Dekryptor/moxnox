#include "kernel.h"
#include "nt.h"

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

NTSTATUS NtShutdownSystem(enum SHUTDOWN_ACTION action) {
    // FIXME - parse ACPI AML to find what constants should be

    switch (action) {
        case SHUTDOWN_ACTION::ShutdownPowerOff:
            outw(0x604, 0x2000); // FIXME - qemu-specific
            break;

        case SHUTDOWN_ACTION::ShutdownReboot:
            outb(0xcf9, 0xf);
            break;

        case SHUTDOWN_ACTION::ShutdownNoReboot:
            // FIXME
            return nt_status::NOT_IMPLEMENTED;
    }

    return nt_status::INTERNAL_ERROR;
}
