module;

#include <stddef.h>
#include "context_offsets.h"

export module kernel_time_page;

import pagesize;
export import timepage;

static_assert(offsetof(time_page_struct, system_time) == TIME_PAGE_SYSTEM_TIME);

export extern "C" {
    time_page_struct kernel_time_page;
}
