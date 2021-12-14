#include "kernel.h"

#define PIT_SCALE 1193180

static const uint8_t pit_interrupt = 0x20;

volatile uint64_t timer_count;

static void outportb(uint16_t port, uint8_t data) {
    asm volatile (
        "out %0, %1"
        :
        : "dN" (port), "a" (data)
    );
}

void test_pit() {
    unsigned int hz, divisor;

    __asm __volatile("cli");

    add_idt_entry(pit_interrupt, (void*)pit_isr, false);

    auto local_apic_id = *(volatile uint32_t*)&local_apic[0x20];

    // IRQ 0 = PIT timer
    add_irq_handler(0, pit_interrupt, local_apic_id);

    hz = 100;
    divisor = PIT_SCALE / hz;

    outportb(0x43, 0x34); // 00 11 010 0 (channel 0, low-byte/high-byte, square wave, bcd = false)
    outportb(0x40, (uint8_t)(divisor & 0xff));
    outportb(0x40, (uint8_t)(divisor >> 8));

    timer_count = 1000;

    __asm __volatile("sti");

    print("Start\n");

    while (timer_count > 0) { }

    print("End\n");

    enable_irq(0, false);
}
