module;

#include <span>
#include <variant>
#include <string_view>
#include <optional>
#include <functional>
#include <atomic>
#include <mutex>
#include "thread.h"

export module thread;

import handle;

using namespace std;

#ifdef __aarch64__

static_assert(offsetof(ARM64_NT_CONTEXT, Cpsr) == CONTEXT_CPSR);
static_assert(offsetof(ARM64_NT_CONTEXT, X0) == CONTEXT_X0);
static_assert(offsetof(ARM64_NT_CONTEXT, Sp) == CONTEXT_SP);
static_assert(offsetof(ARM64_NT_CONTEXT, Pc) == CONTEXT_PC);
static_assert(offsetof(ARM64_NT_CONTEXT, V[0]) == CONTEXT_V0);
static_assert(offsetof(ARM64_NT_CONTEXT, Fpcr) == CONTEXT_FPCR);
static_assert(offsetof(ARM64_NT_CONTEXT, Fpsr) == CONTEXT_FPSR);

static_assert(sizeof(ARM64_NT_CONTEXT) == CONTEXT_SIZEOF);

#elifdef __x86_64__

static_assert(sizeof(AMD64_CONTEXT) == 0x4d0);

static_assert(offsetof(AMD64_CONTEXT, ContextFlags) == CONTEXT_CONTEXT_FLAGS);
static_assert(offsetof(AMD64_CONTEXT, MxCsr) == CONTEXT_MXCSR);
static_assert(offsetof(AMD64_CONTEXT, Rax) == CONTEXT_RAX);
static_assert(offsetof(AMD64_CONTEXT, Rcx) == CONTEXT_RCX);
static_assert(offsetof(AMD64_CONTEXT, Rdx) == CONTEXT_RDX);
static_assert(offsetof(AMD64_CONTEXT, Rbx) == CONTEXT_RBX);
static_assert(offsetof(AMD64_CONTEXT, Rsp) == CONTEXT_RSP);
static_assert(offsetof(AMD64_CONTEXT, Rbp) == CONTEXT_RBP);
static_assert(offsetof(AMD64_CONTEXT, Rsi) == CONTEXT_RSI);
static_assert(offsetof(AMD64_CONTEXT, Rdi) == CONTEXT_RDI);
static_assert(offsetof(AMD64_CONTEXT, R8) == CONTEXT_R8);
static_assert(offsetof(AMD64_CONTEXT, R9) == CONTEXT_R9);
static_assert(offsetof(AMD64_CONTEXT, R10) == CONTEXT_R10);
static_assert(offsetof(AMD64_CONTEXT, R11) == CONTEXT_R11);
static_assert(offsetof(AMD64_CONTEXT, R12) == CONTEXT_R12);
static_assert(offsetof(AMD64_CONTEXT, R13) == CONTEXT_R13);
static_assert(offsetof(AMD64_CONTEXT, R14) == CONTEXT_R14);
static_assert(offsetof(AMD64_CONTEXT, R15) == CONTEXT_R15);
static_assert(offsetof(AMD64_CONTEXT, Rip) == CONTEXT_RIP);
static_assert(offsetof(AMD64_CONTEXT, EFlags) == CONTEXT_EFLAGS);
static_assert(offsetof(AMD64_CONTEXT, SegCs) == CONTEXT_CS);
static_assert(offsetof(AMD64_CONTEXT, SegDs) == CONTEXT_DS);
static_assert(offsetof(AMD64_CONTEXT, SegEs) == CONTEXT_ES);
static_assert(offsetof(AMD64_CONTEXT, SegFs) == CONTEXT_FS);
static_assert(offsetof(AMD64_CONTEXT, SegGs) == CONTEXT_GS);
static_assert(offsetof(AMD64_CONTEXT, SegSs) == CONTEXT_SS);
static_assert(offsetof(AMD64_CONTEXT, FltSave) == CONTEXT_FXSAVE); // must be aligned on 16-byte boundary

#endif

export uint16_t last_tid; // FIXME

export ::thread* get_current_thread();
export void thread_died(::thread& t, bool& last_thread, bool remove_from_proc);
export void terminate_thread(::thread& t, bool remove_from_proc);
export object_ref deref_handle(handle_t h);
export object_ref deref_handle(handle_t h, ACCESS_MASK& access_mask);
export object_ref add_kernel_thread(void (*func)(void*, void*), void* param1, void* param2);

export object_ref add_kernel_thread(void (*func)(void*), void* param) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
    return add_kernel_thread((void(*)(void*, void*))func, param, nullptr);
#pragma GCC diagnostic pop
}

export object_ref add_kernel_thread(void (*func)()) {
    return add_kernel_thread((void(*)(void*, void*))func, nullptr, nullptr);
}
