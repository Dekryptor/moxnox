import nt;
import ntdll;
import format;
import print;

static void reboot() {
    auto status = NtShutdownSystem(SHUTDOWN_ACTION::ShutdownReboot);

    // shouldn't return

    print_stderr("NtShutdownSystem returned {}\n", status);
}

#ifdef __x86_64__
#define FORCE_ALIGN_ARG_POINTER __attribute__((force_align_arg_pointer))
#else
#define FORCE_ALIGN_ARG_POINTER
#endif

extern "C" FORCE_ALIGN_ARG_POINTER void startup() {
    reboot();

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
