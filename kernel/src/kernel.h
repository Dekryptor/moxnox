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

class module_init {
public:
    module_init(void (*func)()) {
        func();
    }
};

// kernel.cpp
extern uint64_t rsdp_pa;
extern bool shutting_down;
extern uintptr_t mn_signal;

class object_ref;

// thread.cpp
object_ref add_kernel_thread(void (*func)());
object_ref add_kernel_thread(void (*func)(void*), void* param);
object_ref add_kernel_thread(void (*func)(void*, void*), void* param1, void* param2);
