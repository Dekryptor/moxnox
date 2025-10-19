module;

#include <stddef.h>

import cpu_block;

export module disable_interrupts;

#ifdef __aarch64__

export inline __attribute__((always_inline)) void enable_interrupts() {
    __asm __volatile("msr DAIFClr, #0xf");
}

export inline __attribute__((always_inline)) void disable_interrupts() {
    __asm __volatile("msr DAIFSet, #0xf");
}

#else

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
#else
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
#else
    __asm __volatile(
        "dec QWORD PTR gs:%0\n"
        :
        : "i" (offsetof(cpu_block, disable_preempt))
    );
#endif
}
