#include <ntdll.h>

import format;
import print;

static void reboot() {
    auto status = NtShutdownSystem(SHUTDOWN_ACTION::ShutdownReboot);

    // shouldn't return

    print("NtShutdownSystem returned {}\n", status);
}

extern "C"
__attribute__((force_align_arg_pointer)) void startup() {
    reboot();

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
