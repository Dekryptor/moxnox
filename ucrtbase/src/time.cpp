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

extern "C"
clock_t clock() {
    LARGE_INTEGER now;

    NtQuerySystemTime(&now);

    return (clock_t)(((uint64_t)now.QuadPart - init_time) / (SYSTEM_TIME_RESOLUTION / CLOCKS_PER_SEC));
}
