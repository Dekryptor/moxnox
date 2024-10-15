#pragma once

#include "context_offsets.h"

class timer : public object {
public:
    timer() : object(object_type::timer) { }
    ~timer();

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    void on_wake();

    NTSTATUS set(int64_t due_time, PTIMER_APC_ROUTINE timer_apc_routine,
                 void* timer_context, bool resume_timer, int32_t period,
                 bool* previous_state);

    bool signalled = false;
    bool active = false;
    list_entry le;
    uint64_t ticks_left;
    uint64_t orig_ticks;
    bool periodic;
    bool autoreset;
};

static_assert(is_sync_object<timer>);
static_assert(object_has_on_wake<timer>);

static_assert(offsetof(timer, le) == TIMER_LE);
static_assert(offsetof(timer, ticks_left) == TIMER_TICKS_LEFT);
