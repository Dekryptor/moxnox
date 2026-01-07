module;

#include <stdint.h>

export module timepage;

import pagesize;

// FIXME - change this to 0x7ffe0000 and make format compatible with Windows
export uintptr_t TIME_PAGE_BASE = 0x7ffd0000;

export struct __attribute__((aligned(PAGE_SIZE))) time_page_struct {
    volatile int64_t system_time;
    uint64_t system_time_offset;
    char os_version[32];
};
