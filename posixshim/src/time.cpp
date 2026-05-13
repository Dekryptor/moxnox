#include <sys/time.h>

#undef errno
import errno;

import nt;
import ucrtbase;

extern "C"
int gettimeofday(struct timeval* tv, void* /*tz*/) {
    LARGE_INTEGER li;

    // FIXME - tz generally ignored?

    if (!tv) {
        *_errno() = EFAULT;
        return -1;
    }

    auto status = NtQuerySystemTime(&li);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    auto t = li.QuadPart; // units of 100ns

    t -= 116444736000000000; // change from NT to Unix epoch
    t /= 10; // change to microseconds

    tv->tv_sec = (long)(t / 1000000);
    tv->tv_usec = (long)(t % 1000000);

    return 0;
}
