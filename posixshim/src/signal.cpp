#include <signal.h>

import print;
import ucrtbase;

#undef errno
import errno;

extern "C"
int sigaction(int /*signum*/, const struct sigaction* /*act*/,
              struct sigaction* /*oldact*/) {
    // FIXME

    return 0;
}

extern "C"
int sigemptyset(sigset_t* set) {
    *set = 0;

    return 0;
}

extern "C"
int sigfillset(sigset_t* set) {
    *set = ~(sigset_t)0;

    return 0;
}

extern "C"
int sigaddset(sigset_t* set, int signum) {
    *set |= (sigset_t)1 << signum;

    return 0;
}

extern "C"
int sigprocmask(int /*how*/, const sigset_t* /*set*/, sigset_t* /*oldset*/) {
    // FIXME

    return 0;
}
