module;

#include <atomic>
#include "timer-asm.h"

module spinlock;

#ifdef __aarch64__

import cpu_block;
import disable_interrupts;

#endif

using namespace std;

#ifdef __aarch64__

template<unsigned U>
void _spinlock<U>::lock() {
    uint32_t current_priority;

    // FIXME - self-modifying code here?

    if (gic_ver == 2) {
        disable_preempt();

        auto& cb = *get_cpu_block();
        auto& GICC_PMR = *(volatile uint32_t*)(cb.gicc + 0x4);

        current_priority = GICC_PMR;
        GICC_PMR = 0; // FIXME - templatable priority?

        enable_preempt();
    } else {
        __asm __volatile (
            "mrs %x[old_val], ICC_PMR_EL1\n"
            "msr ICC_PMR_EL1, xzr\n"
            : [old_val] "=r" (current_priority)
        );
    }

    while (v.exchange(1, memory_order_acquire)) {
        __asm("yield");
    }

    old_priority = current_priority;
}

template<unsigned U>
void _spinlock<U>::unlock() {
    uint32_t last_priority = old_priority;

    v.exchange(0, memory_order_release);

    if (gic_ver == 2) {
        auto& cb = *get_cpu_block();
        auto& GICC_PMR = *(volatile uint32_t*)(cb.gicc + 0x4);

        GICC_PMR = last_priority;
    } else {
        __asm __volatile (
            "msr ICC_PMR_EL1, %x[val]"
            :
            : [val] "r" (last_priority)
        );
    }
}

template<unsigned U>
void _spinlock<U>::force_unlock() {
    v = 0;
}

template class _spinlock<5>;
template class _spinlock<6>;

void sync_lock::lock() {
    uint32_t current_priority;

    if (gic_ver == 2) {
        disable_preempt();

        auto& cb = *get_cpu_block();
        auto& GICC_PMR = *(volatile uint32_t*)(cb.gicc + 0x4);

        current_priority = GICC_PMR;
        GICC_PMR = 0;

        enable_preempt();
    } else {
        __asm __volatile (
            "mrs %x[old_val], ICC_PMR_EL1\n"
            "msr ICC_PMR_EL1, xzr\n"
            : [old_val] "=r" (current_priority)
        );
    }

    acquire_sync_lock();

    old_priority = current_priority;
}

void sync_lock::unlock() {
    uint32_t last_priority = old_priority;

    release_sync_lock();

    if (gic_ver == 2) {
        auto& cb = *get_cpu_block();
        auto& GICC_PMR = *(volatile uint32_t*)(cb.gicc + 0x4);

        GICC_PMR = last_priority;
    } else {
        __asm __volatile (
            "msr ICC_PMR_EL1, %x[val]"
            :
            : [val] "r" (last_priority)
        );
    }
}

#elifdef __x86_64__

template<unsigned CR8>
void _spinlock<CR8>::lock() {
    uint64_t current_cr8;

    __asm __volatile(
        "mov rax, cr8\n"
        "mov %0, rax\n"
        "mov rax, %1\n"
        "mov cr8, rax\n"
        : "=m" (current_cr8)
        : "i" (CR8)
        : "rax"
    );

    while (v.exchange(1, memory_order_acquire)) {
        __asm("pause");
    }

    old_cr8 = current_cr8;
}

template<unsigned CR8>
void _spinlock<CR8>::unlock() {
    uint64_t last_cr8 = old_cr8;

    v.exchange(0, memory_order_release);

    __asm __volatile(
        "mov rax, %0\n"
        "mov cr8, rax\n"
        :
        : "m" (last_cr8)
        : "rax"
    );
}

template<unsigned CR8>
void _spinlock<CR8>::force_unlock() {
    v = 0;
}

template class _spinlock<5>;
template class _spinlock<6>;

void sync_lock::lock() {
    uint64_t current_cr8;

    __asm __volatile(
        "mov rax, cr8\n"
        "mov %0, rax\n"
        "mov rax, 5\n"
        "mov cr8, rax\n"
        : "=m" (current_cr8)
        :
        : "rax"
    );

    acquire_sync_lock();

    old_cr8 = current_cr8;
}

void sync_lock::unlock() {
    uint64_t last_cr8 = old_cr8;

    release_sync_lock();

    __asm __volatile(
        "mov rax, %0\n"
        "mov cr8, rax\n"
        :
        : "m" (last_cr8)
        : "rax"
    );
}

#endif
