#include <stdint.h>

import ucrtbase;
import print;

__sighandler_t signal(int sig, __sighandler_t func) {
    print_stderr("FIXME - signal (sig = {}, func = {:x})\n", sig, (uintptr_t)func);

    return SIG_ERR;
}
