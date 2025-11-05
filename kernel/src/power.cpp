module;

#include <string_view>
#include "acpica.h"
#include "ints.h"

export module power;

import nt;
import print;
import interrupts;
import vfs;
import pipe;
import disable_interrupts;
import scheduler;
import smp;
import efi;
import loaderblock;

#ifndef __aarch64__
import apic;
#endif

using namespace std;

export bool shutting_down = false;

static void reboot() {
    if (ResetSystem)
        ResetSystem(EFI_RESET_TYPE::Cold, (EFI_STATUS)0, 0, nullptr);
    else
        AcpiReset();
}

static void halt() {
    if (ResetSystem)
        ResetSystem(EFI_RESET_TYPE::Shutdown, (EFI_STATUS)0, 0, nullptr);
    else {
        AcpiEnterSleepStatePrep(5);

        disable_interrupts();

        AcpiEnterSleepState(5);
    }
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

#ifndef __aarch64__ // FIXME
    if (num_cpus > 1) {
        processors_left_to_halt = num_cpus - 1;

        // send IPI to other cores to stop them

        auto icr1 = (volatile uint32_t*)&local_apic[0x300];

        *icr1 = 0xc0000 | halt_interrupt;

        while (processors_left_to_halt > 0) { }
    }
#endif

    disable_preempt();

    enable_interrupts();

    // FIXME - GCC fails without superfluous parameter ("not a constant expression because it refers to an incompletely initialized variable")
    // (after making allocate_stack into a template??)
    print("shutting down\n", "");
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
