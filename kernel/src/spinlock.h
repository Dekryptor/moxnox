#pragma once

#include <atomic>
#include "timer-asm.h"

#ifdef __aarch64__

extern uint64_t gicc;

template<unsigned>
class _spinlock {
public:
    void lock() {
        uint32_t current_priority;
        auto& GICC_PMR = *(volatile uint32_t*)(gicc + 0x4);

        current_priority = GICC_PMR;
        GICC_PMR = 0; // FIXME - templatable priority?

        while (v.exchange(1, std::memory_order_acquire)) {
            __asm("yield");
        }

        old_priority = current_priority;
    }

    void unlock() {
        v.exchange(0, std::memory_order_release);

        auto& GICC_PMR = *(volatile uint32_t*)(gicc + 0x4);

        GICC_PMR = old_priority;
    }

private:
    std::atomic<uint64_t> v = 0;
    uint32_t old_priority;
};

using spinlock = _spinlock<5>;

class sync_lock {
public:
    void lock() {
        acquire_sync_lock();
    }

    void unlock() {
        release_sync_lock();
    }
};

#else

template<unsigned CR8>
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

using spinlock = _spinlock<5>;

class sync_lock {
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
