import nt;
import print;

int main() {
    auto status = NtShutdownSystem(SHUTDOWN_ACTION::ShutdownPowerOff);

    // shouldn't return

    print_stderr("NtShutdownSystem returned {}\n", status);

    return 1;
}
