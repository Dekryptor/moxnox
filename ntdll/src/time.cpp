#include <stdint.h>

import nt;
import timepage;

extern "C"
NTSTATUS NtQuerySystemTime(LARGE_INTEGER* SystemTime) {
    const auto& time_page = *(time_page_struct*)TIME_PAGE_BASE;

    SystemTime->QuadPart = (int64_t)((uint64_t)time_page.system_time + time_page.system_time_offset);

    return nt_status::SUCCESS;
}
