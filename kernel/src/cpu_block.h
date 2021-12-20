#pragma once

#include "context_offsets.h"

struct cpu_block {
    cpu_block* self;
    thread* current_thread;
    uint64_t time_slice_left;
    bool handle_timers;
    amd64_context thread_context;
    unsigned int cpu_no;
    uint64_t preempt_block;
};

static_assert(offsetof(cpu_block, current_thread) == GS_CURRENT_THREAD);
static_assert(offsetof(cpu_block, time_slice_left) == GS_TIME_SLICE_LEFT);
static_assert(offsetof(cpu_block, handle_timers) == GS_HANDLE_TIMERS);
static_assert(offsetof(cpu_block, thread_context) == GS_CONTEXT);
static_assert(offsetof(cpu_block, preempt_block) == GS_PREEMPT_BLOCK);

// timer.S
extern "C" cpu_block* get_cpu_block();
