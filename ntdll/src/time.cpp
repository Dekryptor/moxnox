#include <stdint.h>

import nt;
import ntdll_private;

// FIXME - put these in module and share with kernel
static uintptr_t TIME_PAGE_BASE = 0x7ffd0000;

struct __attribute__((aligned(PAGE_SIZE))) time_page_struct {
    volatile int64_t system_time;
    uint64_t system_time_offset;
};

extern "C"
NTSTATUS NtQuerySystemTime(LARGE_INTEGER* SystemTime) {
    const auto& time_page = *(time_page_struct*)TIME_PAGE_BASE;

    SystemTime->QuadPart = (int64_t)((uint64_t)time_page.system_time + time_page.system_time_offset);

    return nt_status::SUCCESS;
}
