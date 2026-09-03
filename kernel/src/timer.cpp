module;

#include <stdint.h>
#include <stddef.h>
#include <span>
#include "context_offsets.h"

export module timer;

import nt;
import list;
import objects;
import object_ref;
import config;
import thread;
import process;

using namespace std;

export extern "C++" {
    uint32_t timer_irq_no;
}

export const int ONE_MS_TICKS = (int)SYSTEM_TIME_RESOLUTION / 1000;

export class timer : public sync_object {
public:
    timer() : sync_object(object_type::timer, false) { }
    ~timer();

    void release() {
        if (--refcount == 0)
            delete this;
    }

    void on_wake();

    NTSTATUS set(int64_t due_time, PTIMER_APC_ROUTINE timer_apc_routine,
                 void* timer_context, bool resume_timer, int32_t period,
                 bool* previous_state);
    NTSTATUS cancel(bool* current_state);
    NTSTATUS query(TIMER_INFORMATION_CLASS info_class, span<uint8_t> buffer,
                   uint32_t& return_length);

    bool active = false;
    list_entry le;
    uint64_t ticks_left;
    uint64_t orig_ticks;
    bool periodic;
    bool autoreset;
    thread* apc_thread = nullptr;
    PTIMER_APC_ROUTINE apc_routine = nullptr;
    void* apc_context;
    process* timer_proc = nullptr;
};

static_assert(is_sync_object<timer>);
static_assert(object_has_on_wake<timer>);

#ifdef __aarch64__

export extern "C++" {
    uint64_t hardware_tick_rate;
}

#endif

static_assert(offsetof(timer, le) == TIMER_LE);
static_assert(offsetof(timer, ticks_left) == TIMER_TICKS_LEFT);

export extern "C" {
    list_entry active_timer_list = { &active_timer_list, &active_timer_list };
}

#ifdef __x86_64__
export void init_lapic_timer(bool ap);
export uint64_t tsc_freq;
#endif

export bool init_timer();
export object_ref create_timer(bool autoreset);
export void msleep(unsigned int ms);
export void udelay(uint64_t us);
