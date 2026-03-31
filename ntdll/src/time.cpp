#include <stdint.h>
#include <algorithm>

#ifdef __x86_64__
#include <x86intrin.h>
#endif

import nt;
import timepage;

using namespace std;

static const unsigned int SYSTEM_TIME_RESOLUTION = 10000000; // 100ns

extern "C"
NTSTATUS NtQuerySystemTime(LARGE_INTEGER* SystemTime) {
    const auto& time_page = *(time_page_struct*)TIME_PAGE_BASE;

    do {
        auto t1 = (uint64_t)time_page.system_time + time_page.system_time_offset;
        uint64_t delta;

#ifdef __x86_64__
        auto r = __rdtsc() - time_page.system_time_tsc;

        delta = r * SYSTEM_TIME_RESOLUTION / time_page.tsc_freq;
        delta = min(time_page.tick_rate - 1, delta);
#else
        delta = 0;
#endif

        auto t2 = (uint64_t)time_page.system_time + time_page.system_time_offset;

        if (t1 == t2) {
            SystemTime->QuadPart = (int64_t)(t1 + delta);
            return nt_status::SUCCESS;
        }
    } while (true);
}
