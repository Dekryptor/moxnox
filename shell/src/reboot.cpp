import nt;
import ntdll;
import format;
import print;

static void reboot() {
    auto status = NtShutdownSystem(SHUTDOWN_ACTION::ShutdownReboot);

    // shouldn't return

    print_stderr("NtShutdownSystem returned {}\n", status);
}

extern "C" void startup() {
    reboot();

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
