#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

import nt;
import ntdll;
import print;

#undef errno
import errno;

struct _pthread {
    HANDLE h;
    void* return_value;
};

extern "C"
int pthread_key_create(pthread_key_t* /*key*/, void (*/*destructor*/)(void*)) {
    print("FIXME - pthread_key_create\n");

    return EINVAL;
}

extern "C"
int pthread_key_delete(pthread_key_t /*key*/) {
    print("FIXME - pthread_key_delete\n");

    return EINVAL;
}

extern "C"
void* pthread_getspecific(pthread_key_t /*key*/) {
    print("FIXME - pthread_getspecific\n");

    return nullptr;
}

extern "C"
int pthread_setspecific(pthread_key_t /*key*/, const void* /*value*/) {
    print("FIXME - pthread_setspecific\n");

    return EINVAL;
}

extern "C"
int pthread_mutex_init(pthread_mutex_t* /*mutex*/, const pthread_mutexattr_t* /*attr*/) {
    print("FIXME - pthread_mutex_init\n");

    return EINVAL;
}

extern "C"
int pthread_mutex_destroy(pthread_mutex_t* /*mutex*/) {
    print("FIXME - pthread_mutex_destroy\n");

    return EINVAL;
}

extern "C"
int pthread_mutex_lock(pthread_mutex_t* /*mutex*/) {
    print("FIXME - pthread_mutex_lock\n");

    return EINVAL;
}

extern "C"
int pthread_mutex_unlock(pthread_mutex_t* /*mutex*/) {
    print("FIXME - pthread_mutex_unlock\n");

    return EINVAL;
}

extern "C"
int pthread_cond_init(pthread_cond_t* /*cond*/, const pthread_condattr_t* /*attr*/) {
    print("FIXME - pthread_cond_init\n");

    return EINVAL;
}

extern "C"
int pthread_cond_destroy(pthread_cond_t* /*cond*/) {
    print("FIXME - pthread_cond_destroy\n");

    return EINVAL;
}

extern "C"
int pthread_cond_wait(pthread_cond_t* /*cond*/, pthread_mutex_t* /*mutex*/) {
    print("FIXME - pthread_cond_wait\n");

    return EINVAL;
}

extern "C"
int pthread_cond_signal(pthread_cond_t* /*cond*/) {
    print("FIXME - pthread_cond_signal\n");

    return EINVAL;
}

static void thread_start(pthread_t thread, void* (*func)(void*), void* arg) {
    auto& p = *(_pthread*)thread;

    auto ret = func(arg);

    p.return_value = ret;

    RtlExitUserThread(nt_status::SUCCESS);
}

extern "C"
int pthread_create(pthread_t* thread, const pthread_attr_t* /*attr*/,
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

    // FIXME - attr

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

    p->h = h;

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
int pthread_join(pthread_t /*thread*/, void** /*retval*/) {
    print("FIXME - pthread_join\n");

    return EINVAL;
}
