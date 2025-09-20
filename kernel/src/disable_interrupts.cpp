export module disable_interrupts;

#ifdef __aarch64__

export inline __attribute__((always_inline)) void enable_interrupts() {
    // FIXME
}

export inline __attribute__((always_inline)) void disable_interrupts() {
    // FIXME
}

#else

export inline __attribute__((always_inline)) void enable_interrupts() {
    __asm __volatile("sti");
}

export inline __attribute__((always_inline)) void disable_interrupts() {
    __asm __volatile("cli");
}

#endif
