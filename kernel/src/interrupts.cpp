#include "kernel.h"
#include <cpuid.h>
#include <span>

using namespace std;

static const uint32_t CPUID_FEAT_EDX_APIC = 0x200;
static const uint32_t IA32_APIC_BASE_MSR = 0x1b;

static const uint16_t interrupt_cs = 0x8;

typedef struct {
    uint16_t offset_1;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_2;
    uint32_t offset_3;
    uint32_t zero;
} idt_entry;

static volatile uint8_t* apic = (uint8_t*)0xfffffab000000000;
static auto idt = span((idt_entry*)0xfffffac000000000, 256);

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

    if (!map_page(apic_base, (uintptr_t)apic)) {
        print("map_page failed for APIC\n");
        return false;
    }

    uint32_t volatile* spurious_interrupt_reg = (uint32_t*)&apic[0xf0];

    *spurious_interrupt_reg = *spurious_interrupt_reg | 0x100;

    return true;
}

static void init_idt_entry(uint8_t vector, void* func, bool trap) {
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

    // FIXME - int 0 (divide by zero)
    // FIXME - int 1 (debug)
    // FIXME - int 2 (NMI)
    // FIXME - int 3
    // FIXME - int 4 (overflow)
    // FIXME - int 5 (bound range exceeded)
    // FIXME - int 6 (#UD)
    // FIXME - int 7 (device not available)
    init_idt_entry(0x8, (void*)double_fault_isr, true);
    // FIXME - int 9 (coprocessor segment overrun)
    // FIXME - int a (invalid TSS)
    // FIXME - int b (segment not present)
    // FIXME - int c (stack-segment fault)
    init_idt_entry(0xd, (void*)general_protection_fault_isr, true);
    init_idt_entry(0xe, (void*)page_fault_isr, true);
    // FIXME - int 10 (x87 exception)
    // FIXME - int 11 (alignment check)
    // FIXME - int 12 (machine check)
    // FIXME - int 13 (SIMD exception)
    // FIXME - int 14 (virtualization exception)
    // FIXME - int 15 (control protection exception)

    init_idt_entry(0x80, (void*)dummy_isr, false);

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
__attribute((sysv_abi))
void dummy_isr_func() {
    // FIXME
    print("ISR\n");
}

extern "C"
__attribute((sysv_abi))
void page_fault(uint64_t address, uint64_t error_code) {
    // FIXME
    print("Page fault (address = ");
    print_hex(address);
    print(", error code = ");
    print_hex(error_code);
    print(")\n");
}

extern "C"
__attribute((sysv_abi))
void double_fault() {
    // FIXME
    print("Double fault\n");
}

extern "C"
__attribute((sysv_abi))
void general_protection_fault(uint64_t error_code) {
    // FIXME
    print("General protection fault (error code = ");
    print_hex(error_code);
    print(")\n");
}
