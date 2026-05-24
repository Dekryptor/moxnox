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
