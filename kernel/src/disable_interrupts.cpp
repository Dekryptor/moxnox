module;

#include <stddef.h>
#include <stdint.h>

import cpu_block;

export module disable_interrupts;

#ifdef __aarch64__

export uint8_t gic_ver;

export inline __attribute__((always_inline)) void enable_interrupts() {
    __asm __volatile("msr DAIFClr, #0xf");
}

export inline __attribute__((always_inline)) void disable_interrupts() {
    __asm __volatile("msr DAIFSet, #0xf");
}

#elifdef __x86_64__

export inline __attribute__((always_inline)) void enable_interrupts() {
    __asm __volatile("sti");
}

export inline __attribute__((always_inline)) void disable_interrupts() {
    __asm __volatile("cli");
}

#endif

export inline __attribute__((always_inline)) void disable_preempt() {
#ifdef __aarch64__
    static_assert(offsetof(cpu_block, disable_preempt) == 0);

    __asm __volatile (
        "mov x8, #1\n"
        "ldaddal x8, x8, [x18]\n"
        :
        :
        : "x8"
    );
#elifdef __x86_64__
    __asm __volatile(
        "inc QWORD PTR gs:%0\n"
        :
        : "i" (offsetof(cpu_block, disable_preempt))
    );
#endif
}

export inline __attribute__((always_inline)) void enable_preempt() {
#ifdef __aarch64__
    get_cpu_block()->disable_preempt--;
#elifdef __x86_64__
    __asm __volatile(
        "dec QWORD PTR gs:%0\n"
        :
        : "i" (offsetof(cpu_block, disable_preempt))
    );
#endif
}

export bool interrupts_disabled() {
#ifdef __aarch64__
    uint64_t daif = 0;

    __asm __volatile(
        "mrs %x[res], daif"
        : [res] "=r" (daif)
    );

    return daif != 0;
#elifdef __x86_64__
    uint64_t rflags = 0;

    __asm __volatile(
        "pushf\n"
        "pop rax\n"
        "mov %0, rax\n"
        :
        : "m" (rflags)
        : "rax"
    );

    return !(rflags & 0x200); // IF flag not set
#endif
}

#ifdef __aarch64__
export uint32_t get_interrupt_priority() {
    if (gic_ver == 2) {
        auto& cb = *get_cpu_block();

        return *(volatile uint32_t*)(cb.gicc + 0x4);
    } else {
        uint64_t val;

        __asm __volatile (
            "mrs %x[val], ICC_PMR_EL1\n"
            : [val] "=r" (val)
        );

        return (uint32_t)val;
    }
}
#endif
