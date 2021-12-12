#include "kernel.h"
#include <cpuid.h>

static const uint32_t CPUID_FEAT_EDX_APIC = 0x200;

static const uint32_t IA32_APIC_BASE_MSR = 0x1b;

static volatile uint8_t* apic = (uint8_t*)0xfffffab000000000;

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

bool init_interrupts() {
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

    auto volatile spurious_interrupt_reg = (uint32_t*)&apic[0xf0];

    *spurious_interrupt_reg = *spurious_interrupt_reg | 0x100;

    // FIXME - set up IDT

    __asm("sti");

    return true;
}
