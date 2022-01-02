#include "kernel.h"
#include "unique_handle.h"

static const uint8_t serial_interrupt = 0x23;
static const uint16_t com1_port = 0x3f8;
static const unsigned int serial_buffer_pages = 1;

static unique_handle com1_pipe;

static void serial_event_recv(void* port) {
    // FIXME
}

bool init_serial() {
    // FIXME - COM2, COM3, COM4

    if (!init_serial_port(com1_port))
        return true;

    com1_pipe = create_pipe(serial_buffer_pages);
    if (!com1_pipe) {
        print("create_pipe failed\n");
        return false;
    }

    if (!name_object(com1_pipe.get(), "ttyS0")) {
        print("name_object failed\n");
        return false;
    }

    if (!add_kernel_thread(serial_event_recv, (void*)com1_port)) {
        print("add_kernel_thread failed\n");
        return false;
    }

    add_idt_entry(serial_interrupt, (void*)serial_isr, false, 0);

    auto local_apic_id = *(volatile uint32_t*)&local_apic[0x20];

    // IRQ 4 = COM1 and COM3
    add_irq_handler(4, serial_interrupt, local_apic_id);

    return true;
}
