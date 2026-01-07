module;

#include <stddef.h>
#include "tick_rate.h"

export module config;

export import pagesize;

export constexpr unsigned int KERNEL_STACK_PAGES = 8; // 32KB
export constexpr unsigned int KERNEL_GUARD_PAGES = 1;

// tick_rate.h
export constexpr unsigned int TICK_RATE = _TICK_RATE;
export constexpr unsigned int SYSTEM_TIME_RESOLUTION = _SYSTEM_TIME_RESOLUTION;
