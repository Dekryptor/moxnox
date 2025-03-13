#include "kernel.h"
#include "acpica.h"
#include "objects.h"
#include "pipe.h"

import print;
import inout;

using namespace std;

static void reboot() {
    AcpiReset();
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
