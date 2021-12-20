#include "kernel.h"
#include "unique_handle.h"

static const uint8_t keyboard_interrupt = 0x21;

unique_handle keyboard_event;

void init_keyboard() {
    add_idt_entry(keyboard_interrupt, (void*)keyboard_isr, false);

    auto local_apic_id = *(volatile uint32_t*)&local_apic[0x20];

    // IRQ 1 = keyboard
    add_irq_handler(1, keyboard_interrupt, local_apic_id);

    keyboard_event = create_event(true);
    if (!keyboard_event) {
        print("create_event failed\n");
        return;
    }
}

extern "C"
void keyboard_isr_func(uint8_t code) {
    print("Keyboard ");
    print_hex(code);
    print("\n");

    // FIXME - add to buffer

    set_event(keyboard_event.get());
}
