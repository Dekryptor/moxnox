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
