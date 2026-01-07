module;

#include <stdint.h>
#include <stddef.h>
#include "context_offsets.h"

export module timepage;

import config;

// FIXME - change this to 0x7ffe0000 and make format compatible with Windows
export uintptr_t TIME_PAGE_BASE = 0x7ffd0000;

struct __attribute__((aligned(PAGE_SIZE))) time_page_struct {
    volatile int64_t system_time;
    uint64_t system_time_offset;
};

static_assert(offsetof(time_page_struct, system_time) == TIME_PAGE_SYSTEM_TIME);

export extern "C" {
    time_page_struct kernel_time_page;
}
