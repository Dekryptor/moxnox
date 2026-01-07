module;

#include <stddef.h>

export module pagesize;

export constexpr unsigned int PAGE_SHIFT = 12;
export constexpr size_t PAGE_SIZE = 1 << PAGE_SHIFT;

export constexpr unsigned int HUGE_PAGE_SHIFT = 21;
export constexpr size_t HUGE_PAGE_SIZE = 1 << HUGE_PAGE_SHIFT;
