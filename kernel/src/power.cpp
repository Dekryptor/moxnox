module;

#include <string_view>
#include "acpica.h"
#include "scheduler.h"
#include "ints.h"

export module power;

import print;
import inout;
import interrupts;
import vfs;
import smp;
import apic;
import pipe;

using namespace std;

export bool shutting_down = false;

static void reboot() {
    AcpiReset();
}

static void halt() {
    AcpiEnterSleepStatePrep(5);

    disable_interrupts();

    AcpiEnterSleepState(5);
}

// work round ICE on GCC 15
static NTSTATUS _NtShutdownSystem(enum SHUTDOWN_ACTION action) {
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

    disable_interrupts();

    if (num_cpus > 1) {
        processors_left_to_halt = num_cpus - 1;

        // send IPI to other cores to stop them

        auto icr1 = (volatile uint32_t*)&local_apic[0x300];

        *icr1 = 0xc0000 | halt_interrupt;

        while (processors_left_to_halt > 0) { }
    }

    disable_preempt();

    enable_interrupts();

    print("shutting down\n");
    shutdown_pipes();

    shutting_down = true;

    if (action == SHUTDOWN_ACTION::ShutdownPowerOff)
        halt();
    else
        reboot();

    enable_preempt();

    return nt_status::INTERNAL_ERROR;
}

extern "C"
NTSTATUS NtShutdownSystem(enum SHUTDOWN_ACTION action) {
    return _NtShutdownSystem(action);
}
