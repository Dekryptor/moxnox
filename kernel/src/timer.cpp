module;

#include "kernel.h"
import objects;
#include "process.h"
#include "thread.h"
#include "spinlock.h"
#include "tick_rate.h"
#include "handle.h"
#include "scheduler.h"

export module timer;

import print;
import list;
import interrupts;
import bsod;
import inout;

using namespace std;

export class timer : public sync_object {
public:
    timer() : sync_object(object_type::timer, false) { }
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
    NTSTATUS cancel(bool* current_state);

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

static const uint8_t timer_interrupt = 0x20;
static const uint8_t timer_ap_interrupt = 0x24;

static const unsigned int TIMER_PERIOD_RESOLUTION = 1000; // milliseconds

volatile uint64_t timer_count;

uint32_t apic_timer_freq;

export extern "C" {
    list_entry active_timer_list = { &active_timer_list, &active_timer_list };

    // FIXME - this should be in a page that userspace can read directly
    volatile int64_t system_time = 0;
    uint64_t system_time_offset = 0;
}


export void init_lapic_timer(bool ap) {
    if (ap) {
        auto timer_div = (volatile uint32_t*)&local_apic[0x3e0];
        *timer_div = 3; // 16
    }

    auto lvt_timer = (volatile uint32_t*)&local_apic[0x320];
    auto timer_initial = (volatile uint32_t*)&local_apic[0x380];

    if (ap)
        *lvt_timer = 0x20000 | timer_ap_interrupt; // periodic
    else
        *lvt_timer = 0x20000 | timer_interrupt; // periodic

    *timer_initial = apic_timer_freq / TICK_RATE;
}

export void pit_delay(uint16_t delay) {
    outb(0x43, 0x30); // initialize channel 0 of PIT
    outb(0x40, (uint8_t)(delay & 0xff)); // push lower byte of param
    outb(0x40, (uint8_t)(delay >> 8)); // push higher byte of param

    while (true) {
        outb(0x43, 0xe2); // check PIT status

        if (inb(0x40) & 0x80) // loop again if not yet signalled
            break;
    }
}

export void init_timer() {
    __asm __volatile("cli");

    // setup APIC timer

    auto lvt_timer = (volatile uint32_t*)&local_apic[0x320];
    *lvt_timer = 0xff; // one shot, vector ff (spurious interrupt)

    auto timer_div = (volatile uint32_t*)&local_apic[0x3e0];
    *timer_div = 3; // 16

    auto timer_initial = (volatile uint32_t*)&local_apic[0x380];
    *timer_initial = 0xffffffff;

    pit_delay(PIT_HZ / 100);

    *lvt_timer = 0x10000; // disable

    auto timer_current = (volatile uint32_t*)&local_apic[0x390];

    apic_timer_freq = (0xffffffff - *timer_current) * 100;

    add_idt_entry(timer_interrupt, (void*)timer_isr, false, 1, false);
    add_idt_entry(timer_ap_interrupt, (void*)timer_isr_ap, false, 1, false);

    init_lapic_timer(false);
}

export object_ref create_timer(bool autoreset) {
    auto tptr = new timer;
    if (!tptr) {
        print("new failed\n");
        return {};
    }

    auto& t = *tptr;
    t.autoreset = autoreset;

    return t;
}

extern "C"
void timer_expired(timer* tptr) {
    // interrupts disabled and sync_lock held

    auto& t = *tptr;

    // signal waiting threads

    auto waiter_count = wake_up_waiters(t.waiters);

    // if autoreset, only signal if nothing waiting when fired
    if (!t.autoreset || waiter_count == 0)
        t.signalled = true;

    if (!t.periodic) {
        t.active = false;
        remove_entry_list(t.le);
        return;
    }

    t.ticks_left = t.orig_ticks;
    t.signalled = false;
}

void timer::on_wake() {
    if (autoreset)
        signalled = false;
}

timer::~timer() {
    if (!active)
        return;

    {
        sync_lock sl;
        lock_guard lock(sl);

        remove_entry_list(le);
    }
}

extern "C"
NTSTATUS NtQuerySystemTime(LARGE_INTEGER* SystemTime) {
    SystemTime->QuadPart = (int64_t)((uint64_t)system_time + system_time_offset);

    return nt_status::SUCCESS;
}

extern "C"
NTSTATUS NtCreateTimer(HANDLE* timer_handle, ACCESS_MASK, const OBJECT_ATTRIBUTES*,
                       TIMER_TYPE timer_type) {
    auto t = get_current_thread();
    auto p = t->proc;

    // FIXME - desired_access
    // FIXME - object_attributes

    if (timer_type != TIMER_TYPE::NotificationTimer && timer_type != TIMER_TYPE::SynchronizationTimer)
        return nt_status::INVALID_PARAMETER;

    auto timerh = create_timer(timer_type == TIMER_TYPE::SynchronizationTimer);

    if (!timerh)
        return nt_status::INTERNAL_ERROR;

    handle_t h;

    auto status = p->add_handle(timerh.get(), 0, true, h); // FIXME - access mask
    if (!success(status))
        return status;

    *timer_handle = h;

    return nt_status::SUCCESS;
}

NTSTATUS timer::set(int64_t due_time, PTIMER_APC_ROUTINE, void*, bool,
                    int32_t period, bool* previous_state) {
    uint64_t ticks;

    // FIXME - timer_apc_routine, timer_context, resume_timer

    if (due_time > 0) {
        // FIXME - handle positive due_time (system time)
        return nt_status::INVALID_PARAMETER;
    } else
        ticks = (uint64_t)((-due_time * TICK_RATE) / SYSTEM_TIME_RESOLUTION);

    if (ticks == 0)
        ticks = 1;

    {
        sync_lock sl;
        lock_guard lock(sl);

        if (previous_state)
            *previous_state = signalled;

        ticks_left = ticks;
        signalled = false;
        periodic = period != 0;

        if (periodic)
            orig_ticks = (unsigned int)(period * TICK_RATE) / TIMER_PERIOD_RESOLUTION;

        if (!active) {
            insert_tail_list(active_timer_list, le);
            active = true;
        }
    }

    return nt_status::SUCCESS;
}

NTSTATUS timer::cancel(bool* current_state) {
    sync_lock sl;
    lock_guard lock(sl);

    if (current_state)
        *current_state = signalled;

    ticks_left = 0;
    signalled = false;
    periodic = false;

    if (active) {
        remove_entry_list(le);
        active = false;
    }

    return nt_status::SUCCESS;
}

extern "C"
NTSTATUS NtSetTimer(HANDLE timer_handle, const LARGE_INTEGER* due_time,
                    PTIMER_APC_ROUTINE timer_apc_routine, void* timer_context,
                    bool resume_timer, int32_t period, bool* previous_state) {
    auto objrw = deref_handle(timer_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::timer)
        return nt_status::OBJECT_TYPE_MISMATCH;

    auto& t = static_cast<timer&>(obj);

    return t.set(due_time->QuadPart, timer_apc_routine, timer_context,
                 resume_timer, period, previous_state);
}

extern "C"
NTSTATUS NtCancelTimer(HANDLE timer_handle, bool* current_state) {
    auto objrw = deref_handle(timer_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::timer)
        return nt_status::OBJECT_TYPE_MISMATCH;

    auto& t = static_cast<timer&>(obj);

    return t.cancel(current_state);
}

extern "C"
NTSTATUS NtDelayExecution(bool alertable, const LARGE_INTEGER* us_delay_interval) {
    print("FIXME - NtDelayExecution\n"); // FIXME

    return nt_status::NOT_IMPLEMENTED;
}
