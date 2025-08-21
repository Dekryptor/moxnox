#pragma once

#include <stdint.h>

static const unsigned int KERNEL_STACK_PAGES = 8; // 32KB
static const unsigned int KERNEL_GUARD_PAGES = 1;

enum class mapping_type {
    no_access,
    read_write,
    read_only,
    read_execute,
    read_write_execute,
    no_cache,
    write_combine,
    reserved
};

// kernel.cpp
extern uint64_t rsdp_pa;
extern bool shutting_down;
extern uintptr_t mn_signal;
