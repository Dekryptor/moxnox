module;

#include <atomic>

export module spinlock;

using namespace std;

extern "C" {
    export uint32_t sync_lock_cpu = 0xffffffff;
}

#ifdef __aarch64__

export template<unsigned>
class _spinlock {
public:
    void lock();
    void unlock();
    void force_unlock();

private:
    atomic<uint64_t> v = 0;
    uint32_t old_priority;
};

export using spinlock = _spinlock<5>;

export class sync_lock {
public:
    void lock();
    void unlock();

private:
    uint32_t old_priority;
};

#elifdef __x86_64__

export template<unsigned CR8>
class _spinlock {
public:
    void lock();
    void unlock();
    void force_unlock();

private:
    atomic<uint64_t> v = 0;
    uint64_t old_cr8;
};

export using spinlock = _spinlock<5>;

export class sync_lock {
public:
    void lock();
    void unlock();

private:
    uint64_t old_cr8;
};

#endif
