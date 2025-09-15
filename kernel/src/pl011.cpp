module;

#include <stdint.h>
#include <utility>
#include <optional>

export module pl011;

import module_init;
import acpi;
import print;

using namespace std;

static void found_pl011(const acpi_device& d);

static acpi_recognizer mod_pnp_hids[] = {
    { {}, "ARMH0011", found_pl011, nullptr },
};

static void init_acpi_hids() {
    for (unsigned int i = 0; i < sizeof(mod_pnp_hids) / sizeof(*mod_pnp_hids); i++) {
        insert_tail_list(pnp_hids, mod_pnp_hids[i].le);
    }
}

static module_init module_init_keyboard(init_acpi_hids);

static const uint32_t PL011_UARTCR_UARTEN = 1 << 0;
static const uint32_t PL011_UARTCR_TXE = 1 << 8;

static const uint32_t PL011_UARTFR_BUSY = 1 << 3;

static const uint32_t PL011_UARTLCR_H_FEN = 1 << 4;
static const unsigned int PL011_UARTLCR_H_WLEN_OFFSET = 5;
static const uint32_t PL011_UARTLCR_H_WLEN_MASK = 3 << PL011_UARTLCR_H_WLEN_OFFSET;

static uint64_t pl011_address; // FIXME

static void pl011_init(uint64_t base_address, uint64_t base_clock,
                       uint32_t baud_rate) {
    pl011_address = base_address;

    auto& UARTFR = *(volatile uint32_t*)(base_address + 0x18);
    auto& UARTIBRD = *(volatile uint32_t*)(base_address + 0x24);
    auto& UARTFBRD = *(volatile uint32_t*)(base_address + 0x28);
    auto& UARTLCR_H = *(volatile uint32_t*)(base_address + 0x2c);
    auto& UARTCR = *(volatile uint32_t*)(base_address + 0x30);
    auto& UARTIMSC = *(volatile uint32_t*)(base_address + 0x38);
    auto& UARTDMACR = *(volatile uint32_t*)(base_address + 0x48);

    // disable UART
    UARTCR &= ~PL011_UARTCR_UARTEN;

    while (UARTFR & PL011_UARTFR_BUSY) { }

    // disable FIFOs
    UARTLCR_H &= ~PL011_UARTLCR_H_FEN;

    auto div = 4 * base_clock / baud_rate;

    UARTIBRD = (div >> 6) & 0xffff;
    UARTFBRD = div & 0x3f;

    // set word length to 8 bits
    UARTLCR_H &= ~(PL011_UARTLCR_H_WLEN_MASK);
    UARTLCR_H |= 3 << PL011_UARTLCR_H_WLEN_OFFSET;

    // mask interrupts
    UARTIMSC = 0x7ff;

    // disable DMA
    UARTDMACR = 0;

    // enable transmission and UART
    UARTCR |= PL011_UARTCR_TXE | PL011_UARTCR_UARTEN;
}

static void uart_write_blocking(char ch) {
    auto& UARTDR = *(volatile uint32_t*)pl011_address;
    auto& UARTFR = *(volatile uint32_t*)(pl011_address + 0x18);

    while (UARTFR & PL011_UARTFR_BUSY) { }

    UARTDR = (uint32_t)ch;
}

static bool init_pl011(uint64_t mem_start, uint64_t mem_length) {
    // FIXME

    pl011_init(mem_start, 24000000, 115200); // FIXME

    uart_write_blocking('!');

    return true;
}

static void found_pl011(const acpi_device& d) {
    optional<pair<uint64_t, uint64_t>> mem;

    for (auto le = d.resources.next; le != &d.resources; le = le->next) {
        auto& r = containing_record<acpi_device_resource>(le);

        // FIXME - interrupt

        if (r.type == acpi_device_resource_type::fixed_memory32) {
            auto& r2 = static_cast<acpi_device_resource_fixed_memory32&>(r);

            if (!mem.has_value())
                mem = make_pair(r2.fixed_memory32.Address, r2.fixed_memory32.AddressLength);
        }
    }

    if (!mem.has_value())
        return;

    if (!init_pl011(mem->first, mem->second))
        print("init_pl011 failed for {:x},{:x}\n", mem->first, mem->second);
}
