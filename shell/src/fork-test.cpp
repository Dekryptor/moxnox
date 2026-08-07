#include <sys/wait.h>
#include <unistd.h>

import print;

static int global = 0x11111111;

int main() {
    int local = 0x22222222;

    auto pid = fork();

    if (pid < 0) {
        print("fork failed ({})\n", pid);
        return 1;
    }

    if (pid == 0) {
        print("child: pid {}, global {:x}, local {:x}\n", getpid(), global, local);

        bool ok = global == 0x11111111 && local == 0x22222222;

        global = 0x33333333;
        local = 0x44444444;

        _exit(ok ? 0 : 1);
    }

    int status = -1;
    auto waited = waitpid(pid, &status, 0);

    print("parent: waitpid returned {} (status {:x}), global {:x}, local {:x}\n",
          waited, status, global, local);

    bool ok = waited == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
              global == 0x11111111 && local == 0x22222222;

    print("{}\n", ok ? "OK" : "FAILED");

    return ok ? 0 : 1;
}
