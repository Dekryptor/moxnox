#include "kernel.h"
#include "objects.h"

using namespace std;

static const uint8_t timer_interrupt = 0x70;

static const unsigned int tick_rate = 1000; // Hz

volatile uint64_t timer_count;

uint32_t apic_timer_freq;

list_entry active_timer_list = { &active_timer_list, &active_timer_list };

void init_lapic_timer(bool set_div) {
    if (set_div) {
        auto timer_div = (volatile uint32_t*)&local_apic[0x3e0];
        *timer_div = 3; // 16
    }

    auto lvt_timer = (volatile uint32_t*)&local_apic[0x320];
    auto timer_initial = (volatile uint32_t*)&local_apic[0x380];

    *lvt_timer = 0x20000 | timer_interrupt; // periodic
    *timer_initial = apic_timer_freq / tick_rate;
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

    print("apic_timer_freq = ");
    print_hex(apic_timer_freq);
    print("\n");

    add_idt_entry(timer_interrupt, (void*)timer_isr, false);

    init_lapic_timer(false);
}

bool create_timer(handle_t* h) {
    auto tptr = new timer;
    if (!tptr) {
        print("new failed\n");
        return false;
    }

    auto& t = *tptr;

    *h = add_handle(t, 0); // FIXME - access mask

    return true;
}

void set_timer(handle_t h, uint64_t timeout) {
    // FIXME - periodic timers

    auto objrw = deref_handle(h);
    if (!objrw) {
        // FIXME - exception
        print("No such handle\n");
        return;
    }

    auto& obj = objrw->get();

    if (obj.type != object_type::timer) {
        // FIXME - exception
        print("Not a timer\n");
        return;
    }

    auto& t = static_cast<timer&>(objrw->get());

    // FIXME - remove from list if already there

    t.ticks_left = timeout; // FIXME - should be 100ns units (and floor at one)

    acquire_sync_lock();
    insert_tail_list(active_timer_list, t.le);
    release_sync_lock();
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
            }

            le = le->next;
        }
    }

    // FIXME - how can we do this outside of lock?
    for (unsigned int i = 0; i < release_count; i++) {
        t.release();
    }

    // FIXME - start going again if periodic, and leave in list

    remove_entry_list(t.le);
}
