#include <sys/resource.h>
#include <errno.h>

import print;

#undef errno
import errno;

extern "C"
int getrlimit(int resource, struct rlimit* rlp) {
    switch (resource) {
        // FIXME - RLIMIT_NOFILE
        // FIXME - RLIMIT_DATA
        // FIXME - RLIMIT_CORE
        // FIXME - RLIMIT_CPU
        // FIXME - RLIMIT_FSIZE
        // FIXME - RLIMIT_STACK

        case RLIMIT_AS:
            rlp->rlim_cur = RLIM_INFINITY;
            rlp->rlim_max = RLIM_INFINITY;

            return 0;

        default:
            print("FIXME - getrlimit {}\n", resource);

            *_errno() = EINVAL;

            return -1;
    }
}
