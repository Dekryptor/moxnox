#include <sys/time.h>

#undef errno
import errno;

import nt;
import ntdll;
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

static int clock_gettime_monotonic(timespec& tp) {
    LARGE_INTEGER count, freq;

    if (!RtlQueryPerformanceCounter(&count)) {
        *_errno() = EINVAL;
        return -1;
    }

    if (!RtlQueryPerformanceFrequency(&freq)) {
        *_errno() = EINVAL;
        return -1;
    }

    tp.tv_sec = count.QuadPart / freq.QuadPart;

    auto frac = (uint64_t)(count.QuadPart % freq.QuadPart);

    tp.tv_nsec = (long)((frac * 1000000000) / (uint64_t)freq.QuadPart);

    return 0;
}

static int clock_gettime_realtime(timespec& tp) {
    LARGE_INTEGER li;

    auto status = NtQuerySystemTime(&li);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    auto t = (uint64_t)li.QuadPart; // units of 100ns

    t -= 116444736000000000; // change from NT to Unix epoch

    tp.tv_sec = t / 10000000;
    tp.tv_nsec = (long)((t % 10000000) * 100);

    return 0;
}

extern "C"
int clock_gettime(clockid_t clockid, timespec* tp) {
    switch (clockid) {
        case CLOCK_MONOTONIC:
            return clock_gettime_monotonic(*tp);

        case CLOCK_REALTIME:
            return clock_gettime_realtime(*tp);

        // FIXME - CLOCK_PROCESS_CPUTIME_ID
        // FIXME - CLOCK_THREAD_CPUTIME_ID

        default:
            *_errno() = EINVAL;
            return -1;
    }
}
