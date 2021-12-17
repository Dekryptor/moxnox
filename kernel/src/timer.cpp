#include "kernel.h"

static const uint8_t timer_interrupt = 0x70;

static const unsigned int tick_rate = 1000; // Hz

volatile uint64_t timer_count;

uint32_t apic_timer_freq;

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

    *lvt_timer = 0x20000 | timer_interrupt; // periodic
    *timer_initial = apic_timer_freq / tick_rate;
}
