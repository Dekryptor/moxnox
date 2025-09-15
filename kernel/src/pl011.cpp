module;

#include <stdint.h>
#include <utility>
#include <optional>
#include <span>
#include <string_view>
#include <atomic>
#include <charconv>
#include <nt.h>
#include "handle.h"

export module pl011;

import module_init;
import acpi;
import print;
import pipe;
import object_ref;
import format;
import mem;

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

static const unsigned int serial_buffer_pages = 1;
static atomic<unsigned int> pl011_number = 0;

static const uint32_t PL011_UARTCR_UARTEN = 1 << 0;
static const uint32_t PL011_UARTCR_TXE = 1 << 8;

static const uint32_t PL011_UARTFR_BUSY = 1 << 3;

static const uint32_t PL011_UARTLCR_H_FEN = 1 << 4;
static const unsigned int PL011_UARTLCR_H_WLEN_OFFSET = 5;
static const uint32_t PL011_UARTLCR_H_WLEN_MASK = 3 << PL011_UARTLCR_H_WLEN_OFFSET;

class pl011_out_pipe : public pipe {
public:
    pl011_out_pipe(unsigned int buffer_pages, uint64_t base_address,
                   unsigned int base_clock, unsigned int baud_rate);
    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override;
    nt_status flush(bool go_readonly) override;

    bool init = false, ready = true;
    uint64_t base_address;
};

nt_status pl011_out_pipe::write(span<const uint8_t> s, uint64_t& written_bytes) {
    auto orig_s = s;

    written_bytes = 0;

    auto& UARTDR = *(volatile uint32_t*)base_address;
    auto& UARTFR = *(volatile uint32_t*)(base_address + 0x18);

    while (!s.empty()) {
        while (UARTFR & PL011_UARTFR_BUSY) { }

        UARTDR = (uint32_t)s[0];
        written_bytes++;

        s = s.subspan(1, s.size() - 1);
    }

    return nt_status::SUCCESS;
}

nt_status pl011_out_pipe::flush(bool go_readonly) {
    // FIXME

    return nt_status::SUCCESS;
}

pl011_out_pipe::pl011_out_pipe(unsigned int buffer_pages, uint64_t pa,
                               unsigned int base_clock, unsigned int baud_rate) : pipe(buffer_pages) {
    // FIXME - unallocate in destructor(?)
    auto va = allocate_va(1);
    if (!va) {
        print("allocate_va failed\n");
        return;
    }

    if (!map_page(pa, va, mapping_type::no_cache)) {
        print("map_page failed\n");
        free_va(va, 1);
        return;
    }

    base_address = va;

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

    init = true;
}

static object_ref create_pl011_out_pipe(unsigned int buffer_pages, uint64_t base_address,
                                        unsigned int base_clock, unsigned int baud_rate) {
    auto pptr = new pl011_out_pipe(buffer_pages, base_address, base_clock,
                                   baud_rate);
    if (!pptr) {
        print("new failed\n");
        return {};
    }

    if (!pptr->init) {
        print("pl011_out_pipe::pl011_out_pipe failed\n");
        delete pptr;
        return {};
    }

    auto& p = *pptr;

    add_to_flushable_pipes_list(*pptr);

    return p;
}

static optional<unsigned int> parse_uid(string_view uid) {
    if (uid.empty())
        return nullopt;

    for (auto c : uid) {
        if (c < '0' || c > '9')
            return nullopt;
    }

    unsigned int val;

    from_chars(uid.data(), uid.data() + uid.size(), val);

    return val;
}

static bool init_pl011(uint64_t mem_start, uint64_t mem_length, string_view uid) {
    if (mem_length == 0) {
        print("init_pl011: invalid mem_length {}\n", mem_length);
        return false;
    }

    // FIXME - interrupts

    // FIXME - where does clock frequency come from?
    auto send_handle = create_pl011_out_pipe(serial_buffer_pages, mem_start,
                                             24000000/*FIXME*/, 115200);
    if (!send_handle) {
        print("create_serial_out_pipe failed\n");
        return false;
    }

    auto& send_pipe = static_cast<pl011_out_pipe&>(send_handle.get());

    // FIXME - input pipe
    // FIXME - create duplex pipe

    // FIXME - numbering assumes that ACPI devices are started in order, i.e. all from one thread

    // use _UID - 1 as number if present, otherwise start from end of numbered range

    auto uidnum = parse_uid(uid);
    unsigned int num;

    if (uidnum.has_value())
        num = *uidnum - 1;
    else
        num = pl011_number.fetch_add(1);

    {
        char s[15];
        auto sp = span(s, sizeof(s));

        sp = format(sp, "ttyAMA{}", num);

        auto status = name_object(send_handle.get(), string_view(s, (size_t)(sp.data() - s)));
        if (!success(status)) {
            print("name_object returned {}\n", status);
            return false;
        }
    }

    // FIXME - set com1_pipe_handle(?)
    // if (num == 0)
        // com1_pipe_handle.swap(send_handle);

    // TESTING
    uint64_t written;
    static const uint8_t msg[] = "hello world\n";
    send_pipe.write(msg, written);

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

    if (!init_pl011(mem->first, mem->second, d.uid))
        print("init_pl011 failed for {:x},{:x}\n", mem->first, mem->second);
}
