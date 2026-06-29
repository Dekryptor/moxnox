#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <utility>
#include <atomic>

import nt;
import ntdll;
import print;

#undef errno
import errno;

using namespace std;

struct _pthread {
    HANDLE h;
    void* return_value;
    RTL_SRWLOCK lock;
    bool detached;
    bool dead;
};

struct _pthread_attr {
    uintptr_t detached : 1;
    uintptr_t : 63;
};

static_assert(sizeof(_pthread_attr) == sizeof(pthread_attr_t));
static_assert(alignof(_pthread_attr) == alignof(pthread_attr_t));

enum class mutex_type : uintptr_t {
    normal = 0,
    recursive = 1,
    errorcheck = 2
};

struct _pthread_mutex_attr {
    mutex_type type : 2;
    uintptr_t : 62;
};

static_assert(sizeof(_pthread_mutex_attr) == sizeof(pthread_mutexattr_t));
static_assert(alignof(_pthread_mutex_attr) == alignof(pthread_mutexattr_t));

struct _pthread_mutex {
    RTL_SRWLOCK lock;
    mutex_type type;
    atomic<uintptr_t> owner;
    unsigned int recursion_count;
};

static_assert(sizeof(_pthread_mutex) == sizeof(pthread_mutex_t));
static_assert(alignof(_pthread_mutex) == alignof(pthread_mutex_t));

struct key_slot {
    bool used;
    void (*dtor)(void*);
};

static constexpr size_t PTHREAD_KEYS_MAX = 128;

static RTL_SRWLOCK key_lock;
static key_slot keys[PTHREAD_KEYS_MAX];
static thread_local void* thread_specific_data[PTHREAD_KEYS_MAX];
static thread_local pthread_t self_pthread;

extern "C"
int pthread_key_create(pthread_key_t* key, void (*dtor)(void*)) {
    RtlAcquireSRWLockExclusive(&key_lock);

    for (size_t i = 0; i < PTHREAD_KEYS_MAX; i++) {
        if (!keys[i].used) {
            keys[i].used = true;
            keys[i].dtor = dtor;
            *key = i;

            RtlReleaseSRWLockExclusive(&key_lock);
            return 0;
        }
    }

    RtlReleaseSRWLockExclusive(&key_lock);

    return EAGAIN;
}

extern "C"
int pthread_key_delete(pthread_key_t key) {
    if (key >= PTHREAD_KEYS_MAX)
        return EINVAL;

    RtlAcquireSRWLockExclusive(&key_lock);

    if (!keys[key].used) {
        RtlReleaseSRWLockExclusive(&key_lock);
        return EINVAL;
    }

    keys[key].used = false;

    RtlReleaseSRWLockExclusive(&key_lock);

    return 0;
}

static void run_dtors() {
    for (unsigned int i = 0; i < 4; i++) {
        bool ran_dtor = false;

        for (size_t j = 0; j < PTHREAD_KEYS_MAX; j++) {
            if (!thread_specific_data[j])
                continue;

            if (!keys[j].used)
                continue;

            if (!keys[j].dtor)
                continue;

            void* v = nullptr;

            swap(v, thread_specific_data[j]);
            keys[j].dtor(v);

            ran_dtor = true;
        }

        if (!ran_dtor)
            break;
    }
}

extern "C"
void* pthread_getspecific(pthread_key_t key) {
    if (key >= PTHREAD_KEYS_MAX)
        return nullptr;

    return thread_specific_data[key];
}

extern "C"
int pthread_setspecific(pthread_key_t key, const void* value) {
    if (key >= PTHREAD_KEYS_MAX)
        return EINVAL;

    thread_specific_data[key] = (void*)value;

    return 0;
}

extern "C"
int pthread_mutex_init(pthread_mutex_t* mutex,
                       const pthread_mutexattr_t* attr) {
    // FIXME - attr

    // FIXME - SRW_LOCKs aren't recursive, will need to account for PTHREAD_MUTEX_RECURSIVE

    auto& m = *(_pthread_mutex*)mutex;

    m.lock.Ptr = nullptr;
    m.type = mutex_type::normal;
    m.owner = 0;
    m.recursion_count = 0;

    if (attr) {
        auto& a = *(_pthread_mutex_attr*)attr;

        m.type = a.type;
    }

    return 0;
}

extern "C"
int pthread_mutex_destroy(pthread_mutex_t* /*mutex*/) {
    // nop

    return 0;
}

extern "C"
int pthread_mutex_lock(pthread_mutex_t* mutex) {
    auto& m = *(_pthread_mutex*)mutex;

    switch (m.type) {
        case mutex_type::normal:
            RtlAcquireSRWLockExclusive(&m.lock);
            break;

        case mutex_type::errorcheck:
            if (m.owner == NtCurrentTeb()->ClientId.UniqueThread)
                return EDEADLK;
            else {
                RtlAcquireSRWLockExclusive(&m.lock);

                m.owner = NtCurrentTeb()->ClientId.UniqueThread;
            }
            break;

        case mutex_type::recursive:
            if (m.owner == NtCurrentTeb()->ClientId.UniqueThread) {
                m.recursion_count++;
                return 0;
            } else {
                RtlAcquireSRWLockExclusive(&m.lock);

                m.owner = NtCurrentTeb()->ClientId.UniqueThread;
                m.recursion_count = 1;
            }
            break;
    }

    return 0;
}

extern "C"
int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    auto& m = *(_pthread_mutex*)mutex;

    switch (m.type) {
        case mutex_type::normal:
            RtlReleaseSRWLockExclusive(&m.lock);
            break;

        case mutex_type::errorcheck:
            if (m.owner != NtCurrentTeb()->ClientId.UniqueThread)
                return EPERM;
            else {
                m.owner = 0;
                RtlReleaseSRWLockExclusive(&m.lock);
            }
            break;

        case mutex_type::recursive:
            if (m.owner != NtCurrentTeb()->ClientId.UniqueThread)
                return EPERM;

            m.recursion_count--;

            if (m.recursion_count == 0) {
                m.owner = 0;
                RtlReleaseSRWLockExclusive(&m.lock);
            }

            break;
    }

    return 0;
}

extern "C"
int pthread_mutex_trylock(pthread_mutex_t* mutex) {
    auto& m = *(_pthread_mutex*)mutex;

    switch (m.type) {
        case mutex_type::normal:
            if (RtlTryAcquireSRWLockExclusive(&m.lock))
                return 0;
            else
                return EBUSY;

        case mutex_type::errorcheck:
            if (m.owner == NtCurrentTeb()->ClientId.UniqueThread)
                return EBUSY;
            else {
                if (RtlTryAcquireSRWLockExclusive(&m.lock)) {
                    m.owner = NtCurrentTeb()->ClientId.UniqueThread;
                    return 0;
                } else
                    return EBUSY;
            }

        case mutex_type::recursive:
            if (m.owner == NtCurrentTeb()->ClientId.UniqueThread) {
                m.recursion_count++;
                return 0;
            } else {
                if (RtlTryAcquireSRWLockExclusive(&m.lock)) {
                    m.owner = NtCurrentTeb()->ClientId.UniqueThread;
                    m.recursion_count = 1;
                    return 0;
                } else
                    return EBUSY;
            }

        default:
            unreachable();
    }
}

extern "C"
int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* /*attr*/) {
    RtlInitializeConditionVariable((RTL_CONDITION_VARIABLE*)cond);

    return 0;
}

extern "C"
int pthread_cond_destroy(pthread_cond_t* /*cond*/) {
    // nop

    return 0;
}

extern "C"
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    auto& m = *(_pthread_mutex*)mutex;

    auto status = RtlSleepConditionVariableSRW((RTL_CONDITION_VARIABLE*)cond,
                                               &m.lock, nullptr, 0);

    m.owner = NtCurrentTeb()->ClientId.UniqueThread;
    m.recursion_count = 1;

    if (!success(status))
        return nt_status_to_errno(status);

    return 0;
}

extern "C"
int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex,
                           const timespec* abstime) {
    LARGE_INTEGER now, wait;
    auto& m = *(_pthread_mutex*)mutex;

    auto status = NtQuerySystemTime(&now);
    if (!success(status))
        return nt_status_to_errno(status);

    auto t = (uint64_t)abstime->tv_sec * 10000000; // units of 100 ns

    t += 116444736000000000; // change from Unix to NT epoch
    t += (uint64_t)abstime->tv_nsec / 100;

    if (t <= (uint64_t)now.QuadPart)
        return ETIMEDOUT;

    wait.QuadPart = now.QuadPart - (int64_t)t;

    status = RtlSleepConditionVariableSRW((RTL_CONDITION_VARIABLE*)cond, &m.lock,
                                          &wait, 0);

    m.owner = NtCurrentTeb()->ClientId.UniqueThread;
    m.recursion_count = 1;

    if (status == nt_status::TIMEOUT)
        return ETIMEDOUT;
    else if (!success(status))
        return nt_status_to_errno(status);

    return 0;
}

extern "C"
int pthread_cond_signal(pthread_cond_t* cond) {
    RtlWakeConditionVariable((RTL_CONDITION_VARIABLE*)cond);

    return 0;
}

extern "C"
int pthread_cond_broadcast(pthread_cond_t* cond) {
    RtlWakeAllConditionVariable((RTL_CONDITION_VARIABLE*)cond);

    return 0;
}

static void thread_start(pthread_t thread, void* (*func)(void*), void* arg) {
    auto& p = *(_pthread*)thread;

    self_pthread = thread;

    auto ret = func(arg);

    p.return_value = ret;

    RtlAcquireSRWLockExclusive(&p.lock);

    if (p.detached) { // if detached, reclaim
        NtClose(p.h);
        RtlReleaseSRWLockExclusive(&p.lock);
        free(&p);
    } else {
        p.dead = true;
        RtlReleaseSRWLockExclusive(&p.lock);
    }

    RtlExitUserThread(nt_status::SUCCESS);
}

extern "C"
int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg) {
    nt_status status;
    HANDLE h;
    CLIENT_ID client_id;
    CONTEXT context;
    INITIAL_TEB initial_teb;
    size_t stack_size;
    void* stack = nullptr;

    auto p = (_pthread*)malloc(sizeof(_pthread));
    if (!p)
        return ENOMEM;

    memset(&initial_teb, 0, sizeof(initial_teb));

    stack_size = 0x100000; // FIXME - should this come from PE header?

    status = NtAllocateVirtualMemory(NtCurrentProcess(), &stack, 0, &stack_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status)) {
        free(p);
        return nt_status_to_errno(status);
    }

    initial_teb.OldStackBase = nullptr;
    initial_teb.OldStackLimit = nullptr;
    initial_teb.StackBase = (uint8_t*)stack + stack_size;
    initial_teb.StackLimit = (uint8_t*)stack + stack_size;
    initial_teb.StackAllocationBase = stack;

    memset(&context, 0, sizeof(context));
#ifdef __aarch64__
    context.Pc = (uintptr_t)thread_start;
    context.Sp = (uintptr_t)initial_teb.StackBase - 0x10;
    context.X0 = (uintptr_t)p;
    context.X1 = (uintptr_t)start_routine;
    context.X2 = (uintptr_t)arg;
#elifdef __x86_64__
    context.Rip = (uintptr_t)thread_start;
    context.Rsp = (uintptr_t)initial_teb.StackBase - 0x28; // 0x20 bytes for MS ABI, 0x8 for return pointer
    context.EFlags = 0x200;
    context.Rcx = (uintptr_t)p;
    context.Rdx = (uintptr_t)start_routine;
    context.R8 = (uintptr_t)arg;
#endif
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;

    status = NtCreateThread(&h, THREAD_ALL_ACCESS, nullptr, NtCurrentProcess(),
                            &client_id, &context, &initial_teb, true);
    if (!success(status)) {
        size_t region_size = 0;

        NtFreeVirtualMemory(NtCurrentProcess(), &stack, &region_size,
                            MEM_RELEASE);
        free(p);
        return nt_status_to_errno(status);
    }

    p->lock.Ptr = nullptr;
    p->h = h;
    p->detached = false;
    p->dead = false;

    if (attr) {
        auto& a = *(_pthread_attr*)attr;

        p->detached = a.detached;
    }

    status = NtResumeThread(h, nullptr);
    if (!success(status)) {
        size_t region_size = 0;

        NtFreeVirtualMemory(NtCurrentProcess(), &stack, &region_size,
                            MEM_RELEASE);
        NtClose(h);
        free(p);
        return nt_status_to_errno(status);
    }

    *thread = (pthread_t)p;

    return 0;
}

extern "C"
int pthread_join(pthread_t thread, void** retval) {
    nt_status status;
    auto& p = *(_pthread*)thread;

    while (true) {
        status = NtWaitForSingleObject(p.h, true, nullptr);

        if (status == nt_status::USER_APC)
            continue;
        else if (!success(status))
            return nt_status_to_errno(status);

        break;
    }

    if (retval)
        *retval = p.return_value;

    NtClose(p.h);
    free(&p);

    return 0;
}

extern "C"
int pthread_detach(pthread_t thread) {
    auto& p = *(_pthread*)thread;

    RtlAcquireSRWLockExclusive(&p.lock);

    if (p.dead) { // already dead, reclaim
        NtClose(p.h);
        RtlReleaseSRWLockExclusive(&p.lock);
        free(&p);
    } else {
        p.detached = true;
        RtlReleaseSRWLockExclusive(&p.lock);
    }

    return 0;
}

extern "C"
pthread_t pthread_self() {
    if (!self_pthread) {
        nt_status status;

        auto p = (_pthread*)malloc(sizeof(_pthread));
        if (!p) {
            print_stderr("malloc failed\n");
            // pthread_self is supposed to always succeed
            NtTerminateProcess(NtCurrentProcess(), nt_status::INSUFFICIENT_RESOURCES);
            unreachable();
        }

        status = NtDuplicateObject(NtCurrentProcess(), NtCurrentThread(),
                                   NtCurrentProcess(), &p->h, 0, 0,
                                   DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);
        if (!success(status)) {
            print_stderr("NtDuplicateObject returned {}\n", status);
            NtTerminateProcess(NtCurrentProcess(), status);
            unreachable();
        }

        p->lock.Ptr = nullptr;
        p->detached = false;
        p->dead = false;

        self_pthread = (pthread_t)p;
    }

    return (pthread_t)self_pthread;
}

extern "C"
[[noreturn]] void pthread_exit(void* retval) {
    auto& p = *(_pthread*)pthread_self();

    p.return_value = retval;

    RtlExitUserThread(nt_status::SUCCESS);
}

extern "C"
int pthread_equal(pthread_t t1, pthread_t t2) {
    return t1 == t2 ? 1 : 0;
}

extern "C"
int pthread_once(pthread_once_t* once_control, void (*init_routine)(void)) {
    atomic_ref<uintptr_t> r(*once_control);
    uintptr_t expected = 0;

    if (r.compare_exchange_strong(expected, 1, memory_order_acquire,
                                  memory_order_acquire)) {
        init_routine();
        r.store(2, memory_order_release);
        RtlWakeAddressAll(once_control);
    } else {
        while (expected != 2) {
            uintptr_t val = 1;

            RtlWaitOnAddress(once_control, &val, sizeof(val), nullptr);
            expected = r.load(memory_order_acquire);
        }
    }

    return 0;
}

extern "C"
int pthread_attr_init(pthread_attr_t* attr) {
    *attr = 0;

    return 0;
}

extern "C"
int pthread_attr_destroy(pthread_attr_t* /*attr*/) {
    // nop

    return 0;
}

extern "C"
int pthread_attr_setdetachstate(pthread_attr_t* attr, int detachstate) {
    if (detachstate != PTHREAD_CREATE_JOINABLE && detachstate != PTHREAD_CREATE_DETACHED)
        return EINVAL;

    auto& a = *(_pthread_attr*)attr;

    if (detachstate == PTHREAD_CREATE_DETACHED)
        a.detached = 1;
    else
        a.detached = 0;

    return 0;
}

extern "C"
int pthread_mutexattr_init(pthread_mutexattr_t* attr) {
    *attr = 0;

    return 0;
}

extern "C"
int pthread_mutexattr_destroy(pthread_mutexattr_t* /*attr*/) {
    // nop

    return 0;
}

extern "C"
int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type) {
    if (type != PTHREAD_MUTEX_NORMAL && type != PTHREAD_MUTEX_ERRORCHECK &&
        type != PTHREAD_MUTEX_RECURSIVE) {
        return EINVAL;
    }

    auto& a = *(_pthread_mutex_attr*)attr;

    switch (type) {
        case PTHREAD_MUTEX_NORMAL:
            a.type = mutex_type::normal;
            break;

        case PTHREAD_MUTEX_ERRORCHECK:
            a.type = mutex_type::errorcheck;
            break;

        case PTHREAD_MUTEX_RECURSIVE:
            a.type = mutex_type::recursive;
            break;
    }

    return 0;
}

__attribute__((section (".CRT$XLD"), used)) void(*__xl_d2)() = run_dtors;
