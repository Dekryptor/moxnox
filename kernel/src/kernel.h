#pragma once

#include <stdint.h>

class thread;

#define offsetofend(TYPE, MEMBER) (offsetof(TYPE, MEMBER) + sizeof(((TYPE *)0)->MEMBER))

static const unsigned int KERNEL_STACK_PAGES = 8; // 32KB
static const unsigned int KERNEL_GUARD_PAGES = 1;

static const uint8_t yield_interrupt = 0xfc;
static const uint8_t halt_interrupt = 0x60;
static const uint8_t invalidate_tlb_interrupt = 0x21;
static const uint8_t spurious_interrupt = 0xff;

using handle_t = uint64_t;

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

// timer.S
extern "C" void timer_isr();
extern "C" void timer_isr_ap();
extern "C" void yield();
extern "C" void acquire_sync_lock();
extern "C" void release_sync_lock();

class object_ref;

// thread.cpp
thread* get_current_thread();
object_ref add_kernel_thread(void (*func)());
object_ref add_kernel_thread(void (*func)(void*), void* param);
object_ref add_kernel_thread(void (*func)(void*, void*), void* param1, void* param2);

#ifdef NDEBUG
#define assert(_Expression) ((void)0)
#else
#define assert(_Expression) \
    (void) \
    ((!!(_Expression)) || \
    (_assert(#_Expression,__FILE__,__LINE__),0))
#endif
