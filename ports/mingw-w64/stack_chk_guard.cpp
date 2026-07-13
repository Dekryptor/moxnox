/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */

// adapted from mingw to avoid ucrtbase

#include <stdint.h>

import ntdll;

void* __stack_chk_guard;

#if defined __SSP__ || defined __SSP_STRONG__ || defined __SSP_ALL__
// This function requires `no_stack_protector` because it changes the
// value of `__stack_chk_guard`, causing stack checks to fail before
// returning from this function.
# if (defined __GNUC__ && __GNUC__ >= 11) || (defined __clang__ && __clang_major__ >= 7)
__attribute__((__no_stack_protector__))
# else
#  error This file can not be built with stack protector enabled. Remove \
         -fstack-protector* options from CFLAGS.
# endif
#endif

__attribute__((__constructor__))
static void init() {
    unsigned int ui;
    uint64_t tsc;
    uint32_t seed;
    uintptr_t val;

    if (__stack_chk_guard)
        return;

#ifdef __x86_64__
    uint32_t lo, hi;

    __asm__ volatile(
        "rdtsc"
        : "=a"(lo), "=d"(hi)
    );

    tsc = ((uint64_t)hi << 32) | lo;
#elifdef __aarch64__
    __asm__ volatile(
        "mrs %0, cntvct_el0"
        : "=r" (tsc)
    );
#else
#error "unhandled architecture"
#endif

    seed = (uint32_t)tsc;

    val = RtlRandom(&seed);
    val = (val << (uint64_t)32) | RtlRandom(&seed);

    __stack_chk_guard = (void*)val;
}
