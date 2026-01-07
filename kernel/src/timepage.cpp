module;

#include <stdint.h>

export module timepage;

import config;

// FIXME - change this to 0x7ffe0000 and make format compatible with Windows
export uintptr_t TIME_PAGE_BASE = 0x7ffd0000;

union time_page_struct {
    struct {
        uint32_t num1;
        uint32_t num2;
        // FIXME
    };
    uint8_t buf[PAGE_SIZE];
};

export time_page_struct kernel_time_page __attribute__((section (".timepage")));
