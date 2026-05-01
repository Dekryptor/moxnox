#include <stdint.h>
#include <sys/types.h>

import nt;
import ucrtbase;

#undef errno
import errno;

extern "C"
int nanosleep(const timespec* duration, timespec* /*rem*/) {
    uint64_t units;
    LARGE_INTEGER t;

    // FIXME - signals?

    // convert to units to 100ns
    units = duration->tv_sec * 10000000;
    units += duration->tv_nsec / 100;

    t.QuadPart = -(int64_t)units;

    auto status = NtDelayExecution(false, &t);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}
