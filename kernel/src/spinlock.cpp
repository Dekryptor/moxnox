module;

#include <atomic>
#include "timer-asm.h"

export module spinlock;

#ifdef __aarch64__

import cpu_block;

export template<unsigned>
class _spinlock {
public:
    void lock() {
        uint64_t daif;

        __asm __volatile (
            "mrs %x[res], daif\n"
            "msr DAIFSet, #0xf\n"
            : [res] "=r" (daif)
        );

        auto& cb = *get_cpu_block();
        uint32_t current_priority;
        auto& GICC_PMR = *(volatile uint32_t*)(cb.gicc + 0x4);

        current_priority = GICC_PMR;
        GICC_PMR = 0; // FIXME - templatable priority?

        __asm __volatile (
            "msr daif, %x[res]"
            :
            : [res] "r" (daif)
        );

        while (v.exchange(1, std::memory_order_acquire)) {
            __asm("yield");
        }

        old_priority = current_priority;
    }

    void unlock() {
        uint32_t last_priority = old_priority;

        auto& cb = *get_cpu_block();

        v.exchange(0, std::memory_order_release);

        auto& GICC_PMR = *(volatile uint32_t*)(cb.gicc + 0x4);

        GICC_PMR = last_priority;
    }

private:
    std::atomic<uint64_t> v = 0;
    uint32_t old_priority;
};

export using spinlock = _spinlock<5>;

export class sync_lock {
public:
    void lock() {
        uint64_t daif;

        __asm __volatile (
            "mrs %x[res], daif\n"
            "msr DAIFSet, #0xf\n"
            : [res] "=r" (daif)
        );

        auto& cb = *get_cpu_block();
        uint32_t current_priority;
        auto& GICC_PMR = *(volatile uint32_t*)(cb.gicc + 0x4);

        current_priority = GICC_PMR;
        GICC_PMR = 0;

        __asm __volatile (
            "msr daif, %x[res]"
            :
            : [res] "r" (daif)
        );

        acquire_sync_lock();

        old_priority = current_priority;
    }

    void unlock() {
        uint32_t last_priority = old_priority;

        auto& cb = *get_cpu_block();

        release_sync_lock();

        auto& GICC_PMR = *(volatile uint32_t*)(cb.gicc + 0x4);

        GICC_PMR = last_priority;
    }

private:
    uint32_t old_priority;
};

#else

export template<unsigned CR8>
class _spinlock {
public:
    void lock() {
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

        while (v.exchange(1, std::memory_order_acquire)) {
            __asm("pause");
        }

        old_cr8 = current_cr8;
    }

    void unlock() {
        uint64_t last_cr8 = old_cr8;

        v.exchange(0, std::memory_order_release);

        __asm __volatile(
            "mov rax, %0\n"
            "mov cr8, rax\n"
            :
            : "m" (last_cr8)
            : "rax"
        );
    }

private:
    std::atomic<uint64_t> v = 0;
    uint64_t old_cr8;
};

export using spinlock = _spinlock<5>;

export class sync_lock {
public:
    void lock() {
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

    void unlock() {
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

private:
    uint64_t old_cr8;
};

#endif
