#include "kernel.h"

static const uint8_t keyboard_interrupt = 0x21;

void init_keyboard() {
    add_idt_entry(keyboard_interrupt, (void*)keyboard_isr, false);

    auto local_apic_id = *(volatile uint32_t*)&local_apic[0x20];

    // IRQ 1 = keyboard
    add_irq_handler(1, keyboard_interrupt, local_apic_id);
}

extern "C"
void keyboard_isr_func(uint8_t code) {
    print("Keyboard ");
    print_hex(code);
    print("\n");

    // FIXME - add to buffer
    // FIXME - wake up any threads waiting for key press
}
