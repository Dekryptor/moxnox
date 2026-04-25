#include <stdint.h>
#include <algorithm>

#ifdef __x86_64__
#include <x86intrin.h>
#endif

import nt;
import timepage;

#ifdef __aarch64__
import aarch64;
#endif

using namespace std;

static const unsigned int SYSTEM_TIME_RESOLUTION = 10000000; // 100ns

extern "C"
NTSTATUS NtQuerySystemTime(LARGE_INTEGER* SystemTime) {
    const auto& time_page = *(time_page_struct*)TIME_PAGE_BASE;

    do {
        auto t1 = (uint64_t)time_page.system_time + time_page.system_time_offset;
        uint64_t r, delta;

#ifdef __x86_64__
        r = __rdtsc();
#elifdef __aarch64__
        r = cntvct_el0_load();
#endif

        r -= time_page.system_time_tsc;

        delta = r * SYSTEM_TIME_RESOLUTION / time_page.tsc_freq;
        delta = min(time_page.tick_rate - 1, delta);

        auto t2 = (uint64_t)time_page.system_time + time_page.system_time_offset;

        if (t1 == t2) {
            SystemTime->QuadPart = (int64_t)(t1 + delta);
            return nt_status::SUCCESS;
        }
    } while (true);
}

extern "C"
bool RtlQueryPerformanceFrequency(LARGE_INTEGER* freq) {
    const auto& time_page = *(time_page_struct*)TIME_PAGE_BASE;

    freq->QuadPart = time_page.tsc_freq;

    return true;
}
