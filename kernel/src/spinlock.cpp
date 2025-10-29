module;

#include <atomic>
#include "timer-asm.h"

export module spinlock;

#ifdef __aarch64__

import cpu_block;
import disable_interrupts;

#endif

using namespace std;

extern "C" {
    export uint32_t sync_lock_cpu = 0xffffffff;
}

#ifdef __aarch64__

export template<unsigned>
class _spinlock {
public:
    void lock() {
        disable_preempt();

        auto& cb = *get_cpu_block();
        uint32_t current_priority;
        auto& GICC_PMR = *(volatile uint32_t*)(cb.gicc + 0x4);

        current_priority = GICC_PMR;
        GICC_PMR = 0; // FIXME - templatable priority?

        enable_preempt();

        while (v.exchange(1, memory_order_acquire)) {
            __asm("yield");
        }

        old_priority = current_priority;
    }

    void unlock() {
        uint32_t last_priority = old_priority;

        auto& cb = *get_cpu_block();

        v.exchange(0, memory_order_release);

        auto& GICC_PMR = *(volatile uint32_t*)(cb.gicc + 0x4);

        GICC_PMR = last_priority;
    }

    void force_unlock() {
        v = 0;
    }

private:
    atomic<uint64_t> v = 0;
    uint32_t old_priority;
};

export using spinlock = _spinlock<5>;

export class sync_lock {
public:
    void lock() {
        uint32_t current_priority;

        disable_preempt();

        auto& cb = *get_cpu_block();
        auto& GICC_PMR = *(volatile uint32_t*)(cb.gicc + 0x4);

        current_priority = GICC_PMR;
        GICC_PMR = 0;

        enable_preempt();

        acquire_sync_lock();

        old_priority = current_priority;
    }

    void unlock() {
        uint32_t last_priority = old_priority;

        release_sync_lock();

        auto& cb = *get_cpu_block();

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

        while (v.exchange(1, memory_order_acquire)) {
            __asm("pause");
        }

        old_cr8 = current_cr8;
    }

    void unlock() {
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

    void force_unlock() {
        v = 0;
    }

private:
    atomic<uint64_t> v = 0;
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
