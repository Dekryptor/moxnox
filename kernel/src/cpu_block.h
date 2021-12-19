#pragma once

#include "context_offsets.h"

struct cpu_block {
    cpu_block* self;
    thread* current_thread;
    uint64_t time_slice_left;
    amd64_context thread_context;
};

static_assert(offsetof(cpu_block, current_thread) == 0x8);
static_assert(offsetof(cpu_block, time_slice_left) == 0x10);
static_assert(offsetof(cpu_block, thread_context) == GS_CONTEXT);

// timer.S
extern "C" cpu_block* get_cpu_block();
