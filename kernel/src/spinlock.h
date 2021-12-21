#pragma once

#include <atomic>

class spinlock {
public:
    void lock() {
        __asm __volatile(
            "mov rax, cr8\n"
            "mov %0, rax\n"
            "mov rax, 3\n"
            "mov cr8, rax\n"
            : "=m" (old_cr8)
            :
            : "rax"
        );

        while (v.exchange(1, std::memory_order_acquire)) {
            __asm("pause");
        }
    }

    void unlock() {
        v.exchange(0, std::memory_order_release);

        __asm __volatile(
            "mov rax, %0\n"
            "mov cr8, rax\n"
            :
            : "m" (old_cr8)
            : "rax"
        );
    }

private:
    std::atomic<uint64_t> v;
    uint64_t old_cr8;
};

static const class {
public:
    static void lock() {
        __asm("cli");
        acquire_sync_lock();
    }

    static void unlock() {
        release_sync_lock();
        __asm("sti");
    }
} sync_lock;
