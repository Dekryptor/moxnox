#pragma once

#include "context_offsets.h"
#include <atomic>
#include <nt.h>

class thread;

struct cpu_block {
    cpu_block* self;
    thread* current_thread;
    uint64_t time_slice_left;
    CONTEXT thread_context;
    uint32_t cpu_no;
    void* scheduler_stack;
#ifdef __aarch64__
    uintptr_t emergency_sp;
    uint64_t emergency_scratch[2];
#else
    void* gdt;
    uint64_t kernel_cr3;
    uint64_t syscall_rax;
#endif
    std::atomic<uint64_t> disable_preempt = 0;
    char* printk_scratch;
};

static_assert(offsetof(cpu_block, current_thread) == GS_CURRENT_THREAD);
static_assert(offsetof(cpu_block, time_slice_left) == GS_TIME_SLICE_LEFT);
static_assert(offsetof(cpu_block, thread_context) == GS_CONTEXT);
static_assert(offsetof(cpu_block, cpu_no) == GS_CPU_NO);
static_assert(offsetof(cpu_block, scheduler_stack) == GS_SCHEDULER_STACK);
static_assert(offsetof(cpu_block, disable_preempt) == GS_DISABLE_PREEMPT);

#ifdef __aarch64__
static_assert(offsetof(cpu_block, emergency_sp) == GS_EMERGENCY_SP);
static_assert(offsetof(cpu_block, emergency_scratch) == GS_EMERGENCY_SCRATCH);
#else
static_assert(offsetof(cpu_block, kernel_cr3) == GS_KERNEL_CR3);
static_assert(offsetof(cpu_block, syscall_rax) == GS_SYSCALL_RAX);
#endif

#ifdef __aarch64__
extern "C++" inline __attribute__((always_inline)) cpu_block* get_cpu_block() {
    uint64_t ret;

    __asm __volatile (
        "mov %x[res], x18"
        : [res] "=r" (ret)
    );

    return (cpu_block*)ret;
}
#else
// timer.S
extern "C" cpu_block* get_cpu_block();
#endif
