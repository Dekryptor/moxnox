module;

#include <time.h>
#include <stdint.h>

export module time;

import nt;
import ucrtbase;

static const unsigned int SYSTEM_TIME_RESOLUTION = 10000000; // 100ns

static uint64_t init_time;

export void init_clock() {
    LARGE_INTEGER now;

    NtQuerySystemTime(&now);

    init_time = (uint64_t)now.QuadPart;
}

export __time64_t win_to_unix_time(int64_t t) {
    return (__time64_t)((t - 116444736000000000) / 10000000);
}

extern "C"
clock_t clock() {
    LARGE_INTEGER now;

    NtQuerySystemTime(&now);

    return (clock_t)(((uint64_t)now.QuadPart - init_time) / (SYSTEM_TIME_RESOLUTION / CLOCKS_PER_SEC));
}

extern "C"
__time64_t __time64(__time64_t* t) {
    LARGE_INTEGER now;
    __time64_t ret;

    NtQuerySystemTime(&now);

    ret = win_to_unix_time(now.QuadPart);

    if (t)
        *t = ret;

    return ret;
}

extern "C"
__time32_t __time32(__time32_t* t) {
    auto ret = (__time32_t)__time64(nullptr);

    if (t)
        *t = ret;

    return ret;
}
