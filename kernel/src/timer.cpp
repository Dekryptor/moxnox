#include "kernel.h"

#define PIT_SCALE 1193182

static const uint8_t pit_interrupt = 0x20;

volatile uint64_t timer_count;

uint32_t apic_timer_freq;

static void outportb(uint16_t port, uint8_t data) {
    asm volatile (
        "out %0, %1"
        :
        : "dN" (port), "a" (data)
    );
}

void init_timer() {
    unsigned int divisor;

    __asm __volatile("cli");

    add_idt_entry(pit_interrupt, (void*)pit_isr, false);

    auto local_apic_id = *(volatile uint32_t*)&local_apic[0x20];

    // IRQ 0 = PIT timer
    add_irq_handler(0, pit_interrupt, local_apic_id);

    // wait for 10 ms

    divisor = PIT_SCALE / 1000;

    outportb(0x43, 0x34); // 00 11 010 0 (channel 0, low-byte/high-byte, rate generator, bcd = false)
    outportb(0x40, (uint8_t)(divisor & 0xff));
    outportb(0x40, (uint8_t)(divisor >> 8));

    timer_count = 10;

    // setup APIC timer

    auto lvt_timer = (volatile uint32_t*)&local_apic[0x320];
    *lvt_timer = 0xff; // one shot, vector ff (spurious interrupt)

    auto timer_div = (volatile uint32_t*)&local_apic[0x3e0];
    *timer_div = 3; // 16

    auto timer_initial = (volatile uint32_t*)&local_apic[0x380];
    *timer_initial = 0xffffffff;

    __asm __volatile("sti");

    while (timer_count > 0) { }

    __asm __volatile("cli");

    *lvt_timer = 0x10000; // disable

    auto timer_current = (volatile uint32_t*)&local_apic[0x390];

    apic_timer_freq = (0xffffffff - *timer_current) * 100;

    print("apic_timer_freq = ");
    print_hex(apic_timer_freq);
    print("\n");

    enable_irq(0, false);

    __asm __volatile("sti");
}
