#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

import nt;
import ntdll;
import ucrtbase;

#undef errno
import errno;

extern "C"
int nanosleep(const timespec* duration, timespec* rem) {
    uint64_t dur_ns, units;
    LARGE_INTEGER t, now;

    if (!duration || duration->tv_sec < 0 || duration->tv_nsec >= 1000000000) {
        *_errno() = EINVAL;
        return -1;
    }

    dur_ns = ((uint64_t)duration->tv_sec * 1000000000) + duration->tv_nsec;

    // convert to units to 100ns

    units = dur_ns / 100;

    if (dur_ns % 100)
        units++;

    t.QuadPart = -(int64_t)units;

    if (rem)
        RtlQueryPerformanceCounter(&now);

    auto status = NtDelayExecution(true, &t);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    if (status == nt_status::USER_APC) {
        if (rem) {
            LARGE_INTEGER then, freq;
            uint64_t dur_ticks, elapsed_ticks, left_ticks;

            RtlQueryPerformanceCounter(&then);
            RtlQueryPerformanceFrequency(&freq);

            dur_ticks = duration->tv_sec * freq.QuadPart;
            dur_ticks += duration->tv_nsec * freq.QuadPart / 1000000000;

            elapsed_ticks = then.QuadPart - now.QuadPart;

            left_ticks = elapsed_ticks < dur_ticks ? dur_ticks - elapsed_ticks : 0;

            rem->tv_sec = left_ticks / freq.QuadPart;
            rem->tv_nsec = (long)(((uint64_t)(left_ticks % freq.QuadPart) * 1000000000) / (uint64_t)freq.QuadPart);
        }

        *_errno() = EINTR;
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
