#pragma once

#include <atomic>

class spinlock {
public:
    void lock() {
        while (v.exchange(1, std::memory_order_acquire)) {
            __asm("pause");
        }
    }

    void unlock() {
        v.exchange(0, std::memory_order_release);
    }

private:
    std::atomic<uint64_t> v;
};
