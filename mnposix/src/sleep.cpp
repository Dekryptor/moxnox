#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

import nt;
import ucrtbase;

#undef errno
import errno;

extern "C"
int nanosleep(const timespec* duration, timespec* /*rem*/) {
    uint64_t units;
    LARGE_INTEGER t;

    // FIXME - signals?

    if (!duration || duration->tv_sec < 0 || duration->tv_nsec >= 1000000000) {
        *_errno() = EINVAL;
        return -1;
    }

    // convert to units to 100ns
    units = (uint64_t)duration->tv_sec * 10000000;
    units += (uint64_t)duration->tv_nsec / 100;

    if (duration->tv_nsec % 100)
        units++;

    t.QuadPart = -(int64_t)units;

    auto status = NtDelayExecution(false, &t);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
unsigned int sleep(unsigned int seconds) {
    timespec t, rem;

    t.tv_sec = seconds;
    t.tv_nsec = 0;

    rem.tv_sec = 0;
    rem.tv_nsec = 0;

    nanosleep(&t, &rem);

    return (unsigned int)rem.tv_sec;
}
