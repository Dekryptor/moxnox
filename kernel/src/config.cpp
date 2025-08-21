module;

#include <stddef.h>

export module config;

export constexpr size_t PAGE_SIZE = 4096;
export constexpr unsigned int PAGE_SHIFT = 12;

export constexpr unsigned int KERNEL_STACK_PAGES = 8; // 32KB
export constexpr unsigned int KERNEL_GUARD_PAGES = 1;
