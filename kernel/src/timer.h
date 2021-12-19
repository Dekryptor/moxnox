#pragma once

#include "context_offsets.h"

class timer : public object {
public:
    timer() : object(object_type::timer) { }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    bool signalled = false;
    list_entry le;
    uint64_t ticks_left;
};

static_assert(is_sync_object<timer>);

static_assert(offsetof(timer, le) == TIMER_LE);
static_assert(offsetof(timer, ticks_left) == TIMER_TICKS_LEFT);
