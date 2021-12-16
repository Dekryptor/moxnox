#include "kernel.h"
#include "acpi.h"
#include "thread.h"
#include <cpuid.h>
#include <span>
#include <optional>
#include <functional>

using namespace std;

static const uint32_t CPUID_FEAT_EDX_APIC = 0x200;
static const uint32_t IA32_APIC_BASE_MSR = 0x1b;

static const uint16_t interrupt_cs = 0x8;

static const uint8_t keyboard_interrupt = 0x21;

typedef struct {
    uint16_t offset_1;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_2;
    uint32_t offset_3;
    uint32_t zero;
} idt_entry;

volatile uint8_t* const local_apic = (uint8_t*)0xfffffab000000000;
static volatile uint8_t* const io_apic = (uint8_t*)0xfffffae000000000;
static auto idt = span((idt_entry*)0xfffffac000000000, 256);
static span<uint8_t> madt;

static bool check_cpu_for_apic() {
    uint32_t eax, ebx, ecx, edx;

    __cpuid(1, eax, ebx, ecx, edx);

    if (!(edx & CPUID_FEAT_EDX_APIC)) {
        print("APIC disabled or not supported by CPU.\n");
        return false;
    }

    return true;
}

static inline uint64_t rdmsr(uint64_t msr) {
    uint32_t low, high;

    __asm __volatile (
        "rdmsr"
        : "=a" (low), "=d" (high)
        : "c"(msr)
    );

    return ((uint64_t)high << 32) | low;
}

static void find_acpi_table(uint32_t sig, invocable<const ACPISDTHeader&> auto func) {
    auto ptr = (uint8_t*)acpi_tables;

    for (unsigned int i = 0; i < acpi_count; i++) {
        auto& tbl = *(ACPISDTHeader*)ptr;

        if (tbl.Signature == sig)
            func(tbl);

        ptr += tbl.Length;
    }
}

static span<uint8_t> find_madt() {
    optional<reference_wrapper<const ACPISDTHeader>> madtw;

    find_acpi_table(0x43495041, [&](const ACPISDTHeader& tbl) { // "APIC"
        if (madtw)
            return;

        madtw = tbl;
    });

    if (!madtw) {
        print("MADT not found in ACPI tables.\n");
        return span<uint8_t>();
    }

    auto& madt = madtw->get();

    if (madt.Length < sizeof(ACPISDTHeader) + sizeof(uint32_t) + sizeof(uint32_t)) {
        print("MADT was shorter than expected.\n");
        return span<uint8_t>();
    }

    return span((uint8_t*)&madt + sizeof(ACPISDTHeader) + sizeof(uint32_t) + sizeof(uint32_t),
                madt.Length - sizeof(ACPISDTHeader) - sizeof(uint32_t) - sizeof(uint32_t));
}

static uint64_t find_ioapic(span<const uint8_t> data) {
    while (data.size() > 2) {
        auto type = data[0];
        auto length = data[1];

        if (type == 1 && length >= 2 + sizeof(madt_ioapic)) { // I/O APIC
            auto& ent = *(madt_ioapic*)&data[2];

            return ent.address;
        }

        data = data.subspan(length);
    }

    print("Could not find address of I/O APIC.\n");

    return 0;
}

static uint32_t get_irq_gsi(uint8_t irq) {
    auto data = madt;

    // check source overrides in MADT

    while (data.size() > 2) {
        auto type = data[0];
        auto length = data[1];

        if (type == 2 && length >= 2 + sizeof(madt_source_override)) { // source override
            auto& ent = *(madt_source_override*)&data[2];

            if (ent.bus_source == 0 && ent.irq_source == irq)
                return ent.gsi;

            break;
        }

        data = data.subspan(length);
    }

    return irq;
}

void add_irq_handler(uint8_t irq, uint8_t vec, uint32_t apic_id) {
    // FIXME - handle multiple I/O APICs (send to right one)

    volatile uint32_t* IOREGSEL = (uint32_t*)io_apic;
    volatile uint32_t* IOREGWIN = (uint32_t*)(io_apic + 0x10);

    auto write_reg = [&](uint32_t reg, uint32_t val) {
        *IOREGSEL = reg;
        *IOREGWIN = val;
    };

    auto gsi = get_irq_gsi(irq);

    write_reg(0x10 + (gsi * 2), vec);
    write_reg(0x11 + (gsi * 2), apic_id);
};

void enable_irq(uint8_t irq, bool enable) {
    // FIXME - handle multiple I/O APICs (send to right one)

    volatile uint32_t* IOREGSEL = (uint32_t*)io_apic;
    volatile uint32_t* IOREGWIN = (uint32_t*)(io_apic + 0x10);

    auto read_reg = [&](uint32_t reg) -> uint32_t {
        *IOREGSEL = reg;

        return *IOREGWIN;
    };

    auto write_reg = [&](uint32_t reg, uint32_t val) {
        *IOREGSEL = reg;
        *IOREGWIN = val;
    };

    auto gsi = get_irq_gsi(irq);

    __asm __volatile("cli");

    auto val = read_reg(0x10 + (gsi * 2));

    if (enable)
        val &= ~0x10000;
    else
        val |= 0x10000;

    write_reg(0x10 + (gsi * 2), val);

    __asm __volatile("sti");
}

static bool init_apic() {
    // FIXME - can we assume that nothing's disabled the APIC? (Check EFI specs?)

    if (!check_cpu_for_apic())
        return false;

    // disable 8259 PIC
    __asm __volatile (
        "mov al, 0xff\n"
        "out 0xa1, al\n"
        "out 0x21, al\n"
        :
        :
        : "al"
    );

    auto apic_base = rdmsr(IA32_APIC_BASE_MSR) & 0xfffff000;

    if (!map_page(apic_base, (uintptr_t)local_apic)) {
        print("map_page failed for local APIC\n");
        return false;
    }

    auto spurious_interrupt_reg = (volatile uint32_t*)&local_apic[0xf0];

    *spurious_interrupt_reg = *spurious_interrupt_reg | 0x100;

    madt = find_madt();
    if (madt.empty())
        return false;

    auto io_apic_pa = find_ioapic(madt);
    if (!io_apic_pa)
        return false;

    if (!map_page(io_apic_pa, (uintptr_t)io_apic)) {
        print("map_page failed for I/O APIC\n");
        return false;
    }

    auto local_apic_id = *(volatile uint32_t*)&local_apic[0x20];

    // IRQ 1 = keyboard
    add_irq_handler(1, keyboard_interrupt, local_apic_id);

    return true;
}

void add_idt_entry(uint8_t vector, void* func, bool trap) {
    auto& i = idt[vector];
    auto addr = (uintptr_t)func;

    i.offset_1 = addr & 0xffff;
    i.selector = interrupt_cs;
    i.ist = 0;

    if (trap)
        i.type_attr = 0x8f; // present = 1, DPL = 0, type = 0xf (trap gate)
    else
        i.type_attr = 0x8e; // present = 1, DPL = 0, type = 0xe (interrupt gate)

    i.offset_2 = (addr >> 16) & 0xffff;
    i.offset_3 = (uint32_t)(addr >> 32);
    i.zero = 0;
}

static bool setup_idt() {
    auto idt_page = allocate_page();
    if (idt_page == 0) {
        print("Could not allocate page for IDT.\n");
        return false;
    }

    if (!map_page(idt_page, (uintptr_t)idt.data())) {
        print("map_page failed for IDT\n");
        return false;
    }

    memset(idt.data(), 0, PAGE_SIZE);

    // FIXME - int 0 (divide by zero)
    // FIXME - int 1 (debug)
    // FIXME - int 2 (NMI)
    // FIXME - int 3
    // FIXME - int 4 (overflow)
    // FIXME - int 5 (bound range exceeded)
    add_idt_entry(0x6, (void*)invalid_opcode_isr, false);
    // FIXME - int 7 (device not available)
    add_idt_entry(0x8, (void*)double_fault_isr, true);
    // FIXME - int 9 (coprocessor segment overrun)
    // FIXME - int a (invalid TSS)
    // FIXME - int b (segment not present)
    // FIXME - int c (stack-segment fault)
    add_idt_entry(0xd, (void*)general_protection_fault_isr, false);
    add_idt_entry(0xe, (void*)page_fault_isr, true);
    // FIXME - int 10 (x87 exception)
    // FIXME - int 11 (alignment check)
    // FIXME - int 12 (machine check)
    // FIXME - int 13 (SIMD exception)
    // FIXME - int 14 (virtualization exception)
    // FIXME - int 15 (control protection exception)

    add_idt_entry(keyboard_interrupt, (void*)keyboard_isr, false);

    add_idt_entry(0x80, (void*)dummy_isr, false);

    add_idt_entry(0xff, (void*)spurious_isr, false);

    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) idt_ptr;

    idt_ptr.base = (uintptr_t)idt.data();
    idt_ptr.limit = (uint16_t)(idt.size_bytes() - 1);

    __asm__ __volatile__ (
        "lidt %0"
        :
        : "m" (idt_ptr)
    );

    return true;
}

bool init_interrupts() {
    if (!setup_idt())
        return false;

    if (!init_apic())
        return false;

    __asm("sti; int 0x80");

    return true;
}

extern "C"
void dummy_isr_func() {
    // FIXME
    print("ISR\n");
}

extern "C"
void page_fault(uint64_t address, uint64_t error_code) {
    // FIXME
    print("Page fault (address = ");
    print_hex(address);
    print(", error code = ");
    print_hex(error_code);
    print(")\n");
}

extern "C"
void double_fault() {
    // FIXME
    print("Double fault\n");
}

extern "C"
void general_protection_fault(uint64_t error_code) {
    print("General protection fault (error code = ");
    print_hex(error_code);

    if (!current_thread) {
        print(", not in thread)\n");
    } else {
        print(", thread ");
        print_hex(current_thread->tid);
        print(")\n");
    }

    print_context(thread_context);

    if (!current_thread) {
        // FIXME - BSOD
        while (1) { }
    } else {
        print("Thread killed\n");
        terminate_thread();
    }
}

extern "C"
void invalid_opcode() {
    print("Invalid opcode");

    if (!current_thread) {
        print(", not in thread\n");
    } else {
        print(", thread ");
        print_hex(current_thread->tid);
        print("\n");
    }

    print_context(thread_context);

    if (!current_thread) {
        // FIXME - BSOD
        while (1) { }
    } else {
        print("Thread killed\n");
        terminate_thread();
    }
}

extern "C"
void keyboard_isr_func() {
    print("Keyboard\n");

    // FIXME - get value and add to buffer
    // FIXME - wake up any threads waiting for key press

    auto eoi = (volatile uint32_t*)&local_apic[0xb0];

    *eoi = 0;
}
