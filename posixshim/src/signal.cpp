#include <signal.h>

import print;
import ucrtbase;

#undef errno
import errno;

extern "C"
int sigaction(int signum, const struct sigaction* act, struct sigaction* oldact) {
    print("FIXME - sigaction ({}, {:x}, {:x})\n", signum, (uintptr_t)act, (uintptr_t)oldact);

    // FIXME

    *_errno() = EINVAL;

    return -1;
}
