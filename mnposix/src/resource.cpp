#include <sys/resource.h>
#include <errno.h>

import print;

#undef errno
import errno;

extern "C"
int getrlimit(int resource, struct rlimit* /*rlp*/) {
    print("FIXME - getrlimit {}\n", resource);

    *_errno() = EINVAL;

    return 0;
}
