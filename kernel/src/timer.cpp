#include "kernel.h"
#include "objects.h"
#include "spinlock.h"
#include "tick_rate.h"
#include "handle.h"

using namespace std;

static const uint8_t timer_interrupt = 0x20;
static const uint8_t timer_ap_interrupt = 0x24;

static const unsigned int TIMER_PERIOD_RESOLUTION = 1000; // milliseconds

volatile uint64_t timer_count;

uint32_t apic_timer_freq;

list_entry active_timer_list = { &active_timer_list, &active_timer_list };

// FIXME - this should be in a page that userspace can read directly
volatile int64_t system_time = 0;

void init_lapic_timer(bool ap) {
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

void init_timer() {
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

    printk("apic_timer_freq = ");
    printk(apic_timer_freq);
    printk("\n");

    add_idt_entry(timer_interrupt, (void*)timer_isr, false, 1);
    add_idt_entry(timer_ap_interrupt, (void*)timer_isr_ap, false, 1);

    init_lapic_timer(false);
}

object_ref create_timer(bool autoreset) {
    auto tptr = new timer;
    if (!tptr) {
        printk("new failed\n");
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
    unsigned int release_count = 0;

    // signal waiting threads

    {
        auto le = thread_list.next;

        while (le != &thread_list) {
            auto& thr = containing_record<thread>(le);

            if (thr.status == thread_status::waiting && thr.waiting_for.has_value() && &thr.waiting_for->get() == static_cast<object*>(&t)) {
                thr.status = thread_status::runnable;
                thr.waiting_for = nullopt;
                release_count++;

                if (t.autoreset)
                    break;
            }

            le = le->next;
        }
    }

    // if autoreset, only signal if nothing waiting when fired
    if (!t.autoreset || release_count == 0)
        t.signalled = true;

    // FIXME - how can we do this outside of lock?
    for (unsigned int i = 0; i < release_count; i++) {
        t.release();
    }

    if (!t.periodic) {
        t.active = false;
        remove_entry_list(t.le);
        t.release();
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

NTSTATUS NtQuerySystemTime(LARGE_INTEGER* SystemTime) {
    SystemTime->QuadPart = system_time;

    return nt_status::SUCCESS;
}

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

    auto h = p->add_handle(timerh.get(), 0); // FIXME - access mask

    if (!h)
        return nt_status::INTERNAL_ERROR;

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
        ticks = (-due_time * TICK_RATE) / SYSTEM_TIME_RESOLUTION;

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
            orig_ticks = (period * TICK_RATE) / TIMER_PERIOD_RESOLUTION;

        if (!active) {
            insert_tail_list(active_timer_list, le);
            active = true;
        }
    }

    return nt_status::SUCCESS;
}

NTSTATUS NtSetTimer(HANDLE timer_handle, const LARGE_INTEGER* due_time,
                    PTIMER_APC_ROUTINE timer_apc_routine, void* timer_context,
                    bool resume_timer, int32_t period, bool* previous_state) {
    auto objrw = deref_handle2(timer_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::timer)
        return nt_status::OBJECT_TYPE_MISMATCH;

    auto& t = static_cast<timer&>(obj);

    return t.set(due_time->QuadPart, timer_apc_routine, timer_context,
                 resume_timer, period, previous_state);
}
