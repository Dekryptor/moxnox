#include <stdint.h>
#include <string.h>
#include <winerror.h>
#include <utility>

import nt;
import kernelbase;
import print;

using namespace std;

extern "C"
uint32_t TlsAlloc() {
    print_stderr("FIXME - TlsAlloc\n");

    return 0;
}

extern "C"
uint32_t GetLastError() {
    return NtCurrentTeb()->LastErrorValue;
}

extern "C"
void* TlsGetValue(uint32_t tls_index) {
    print_stderr("FIXME - TlsGetValue {}\n", tls_index);

    return nullptr;
}

extern "C"
bool TlsSetValue(uint32_t tls_index, void* tls_value) {
    print_stderr("FIXME - TlsSetValue {}, {:x}\n", tls_index, (uintptr_t)tls_value);

    return false;
}

static void thread_start(THREAD_START_ROUTINE func, void* param) {
    auto ret = func(param);

    NtTerminateThread(NtCurrentThread(), (nt_status)ret);
    unreachable();
}

extern "C"
HANDLE CreateThread(SECURITY_ATTRIBUTES* sa, size_t stack_size,
                    THREAD_START_ROUTINE start, void* param,
                    uint32_t flags, uint32_t* id) {
    nt_status status;
    HANDLE h;
    CLIENT_ID client_id;
    CONTEXT context;
    INITIAL_TEB initial_teb;
    void* stack = nullptr;

    static const uint32_t supported_flags = CREATE_SUSPENDED;

    if (sa) {
        print_stderr("FIXME - non-NULL SECURITY_ATTRIBUTES passed to CreateThread\n");
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        return 0;
    }

    if (id) {
        print_stderr("FIXME - non-NULL id pointer passed to CreateThread\n");
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        return 0;
    }

    if (flags & ~supported_flags) {
        print_stderr("FIXME - unsupported CreateThread flags {:x}\n",
                     flags & ~supported_flags);
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        return 0;
    }

    // FIXME - should just be passing to ntdll!RtlCreateUserThread?

    memset(&initial_teb, 0, sizeof(initial_teb));

    if (stack_size == 0)
        stack_size = 0x100000; // FIXME - should this come from PE header?

    status = NtAllocateVirtualMemory(NtCurrentProcess(), &stack, 0, &stack_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status)) {
        print_stderr("NtAllocateVirtualMemory returned {}\n", status);
        // FIXME - last error
        return 0;
    }

    initial_teb.OldStackBase = nullptr;
    initial_teb.OldStackLimit = nullptr;
    initial_teb.StackBase = (uint8_t*)stack + stack_size;
    initial_teb.StackLimit = (uint8_t*)stack + stack_size;
    initial_teb.StackAllocationBase = stack;

    memset(&context, 0, sizeof(context));
#ifdef __aarch64__
    context.Pc = (uintptr_t)thread_start;
    context.Sp = (uintptr_t)initial_teb.StackBase - 0x10; // 0x10 bytes for red zone
    context.X0 = (uintptr_t)start;
    context.X1 = (uintptr_t)param;
#elifdef __x86_64__
    context.Rip = (uintptr_t)thread_start;
    context.Rsp = (uintptr_t)initial_teb.StackBase - 0x28; // 0x20 bytes for MS ABI, 0x8 for return pointer
    context.EFlags = 0x200;
    context.Rcx = (uintptr_t)start;
    context.Rdx = (uintptr_t)param;
#endif
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;

    status = NtCreateThread(&h, THREAD_ALL_ACCESS, nullptr, NtCurrentProcess(), &client_id, &context,
                            &initial_teb, true);

    if (!success(status)) {
        print_stderr("NtCreateThread returned {}\n", status);
        // FIXME - last error
        return 0;
    }

    if (!(flags & CREATE_SUSPENDED))
        NtResumeThread(h, nullptr);

    return h;
}
