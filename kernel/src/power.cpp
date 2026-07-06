module;

#include <string_view>
#include <atomic>
#include <bit>
#include "ints.h"

export module power;

import nt;
import print;
import vfs;
import pipe;
import disable_interrupts;
import efi;
import acpica;
import gnu_efi;
import scheduler;
import cpu_block;
import process;

using namespace std;

export atomic<uint32_t> shutting_down_cpu = 0xffffffff;

static void reboot() {
    AcpiReset();

    if (ResetSystem)
        ResetSystem(EfiResetCold, (EFI_STATUS)0, 0, nullptr);
}

static void halt() {
    AcpiEnterSleepStatePrep(5);

    disable_interrupts();

    AcpiEnterSleepState(5);

    if (ResetSystem)
        ResetSystem(EfiResetShutdown, (EFI_STATUS)0, 0, nullptr);
}

// work round ICE on GCC 15
static NTSTATUS _NtShutdownSystem(enum SHUTDOWN_ACTION action) {
    if (action == SHUTDOWN_ACTION::ShutdownNoReboot) {
        // FIXME
        return nt_status::NOT_IMPLEMENTED;
    }

    if (action != SHUTDOWN_ACTION::ShutdownPowerOff && action != SHUTDOWN_ACTION::ShutdownReboot)
        return nt_status::INVALID_PARAMETER;

    broadcast_sigterm();

    // FIXME - wait a few seconds for everybody to finish up

    // flush filesystems and make readonly
    auto status = flush_filesystems(true);
    if (!success(status))
        return status;

    disable_interrupts();
    disable_preempt();

    uint32_t expected = 0xffffffff;

    if (!shutting_down_cpu.compare_exchange_strong(expected, get_cpu_block()->cpu_no,
                                                   memory_order::release)) {
        enable_preempt();
        enable_interrupts();
        return nt_status::SYSTEM_SHUTDOWN;
    }

    while (popcount(active_cpus.load(memory_order::acquire)) > 1) {
#ifdef __aarch64__
        __asm("yield");
#elifdef __x86_64__
        __asm("pause");
#endif
    }

    enable_interrupts();

    print("shutting down\n");
    shutdown_pipes();

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
