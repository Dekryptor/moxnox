#pragma once

#include <cstdint>
#include "list.h"
#include "context_offsets.h"

static_assert(sizeof(CONTEXT_AMD64) == 0x4d0);

static_assert(offsetof(CONTEXT_AMD64, Rax) == CONTEXT_RAX);
static_assert(offsetof(CONTEXT_AMD64, Rcx) == CONTEXT_RCX);
static_assert(offsetof(CONTEXT_AMD64, Rdx) == CONTEXT_RDX);
static_assert(offsetof(CONTEXT_AMD64, Rbx) == CONTEXT_RBX);
static_assert(offsetof(CONTEXT_AMD64, Rsp) == CONTEXT_RSP);
static_assert(offsetof(CONTEXT_AMD64, Rbp) == CONTEXT_RBP);
static_assert(offsetof(CONTEXT_AMD64, Rsi) == CONTEXT_RSI);
static_assert(offsetof(CONTEXT_AMD64, Rdi) == CONTEXT_RDI);
static_assert(offsetof(CONTEXT_AMD64, R8) == CONTEXT_R8);
static_assert(offsetof(CONTEXT_AMD64, R9) == CONTEXT_R9);
static_assert(offsetof(CONTEXT_AMD64, R10) == CONTEXT_R10);
static_assert(offsetof(CONTEXT_AMD64, R11) == CONTEXT_R11);
static_assert(offsetof(CONTEXT_AMD64, R12) == CONTEXT_R12);
static_assert(offsetof(CONTEXT_AMD64, R13) == CONTEXT_R13);
static_assert(offsetof(CONTEXT_AMD64, R14) == CONTEXT_R14);
static_assert(offsetof(CONTEXT_AMD64, R15) == CONTEXT_R15);
static_assert(offsetof(CONTEXT_AMD64, Rip) == CONTEXT_RIP);
static_assert(offsetof(CONTEXT_AMD64, EFlags) == CONTEXT_EFLAGS);
static_assert(offsetof(CONTEXT_AMD64, SegCs) == CONTEXT_CS);
static_assert(offsetof(CONTEXT_AMD64, SegDs) == CONTEXT_DS);
static_assert(offsetof(CONTEXT_AMD64, SegEs) == CONTEXT_ES);
static_assert(offsetof(CONTEXT_AMD64, SegFs) == CONTEXT_FS);
static_assert(offsetof(CONTEXT_AMD64, SegGs) == CONTEXT_GS);
static_assert(offsetof(CONTEXT_AMD64, SegSs) == CONTEXT_SS);
static_assert(offsetof(CONTEXT_AMD64, FltSave) == CONTEXT_FXSAVE); // must be aligned on 16-byte boundary

enum class thread_status {
    runnable,
    running,
    waiting,
    dead
};

class process;

class thread : public object {
public:
    thread() : object(object_type::thread) { }
    ~thread();

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    void terminate();

    list_entry le;
    bool signalled = false;
    CONTEXT context;
    enum thread_status status;
    uint32_t tid;
    void* kernel_stack = nullptr;
    std::optional<std::reference_wrapper<object>> waiting_for; // FIXME - multiple objects
    process* proc = nullptr;
    uint64_t affinity = 0xffffffffffffffff;
    void* syscall_stack = nullptr;
    bool kernel_mode;
    void* rip_before_syscall;
    void* rsp_before_syscall;
    uint64_t rflags_before_syscall;
    enum nt_status exit_status = nt_status::PENDING;
    list_entry process_le;
    std::atomic<unsigned int> suspend_count = 0;
    TEB* teb;
};

static_assert(is_sync_object<thread>);

static_assert(offsetof(thread, proc) == THREAD_PROC);
static_assert(offsetof(thread, syscall_stack) == THREAD_SYSCALL_STACK);
static_assert(offsetof(thread, kernel_mode) == THREAD_KERNEL_MODE);
static_assert(offsetof(thread, rip_before_syscall) == THREAD_RIP_BEFORE_SYSCALL);
static_assert(offsetof(thread, rsp_before_syscall) == THREAD_RSP_BEFORE_SYSCALL);
static_assert(offsetof(thread, rflags_before_syscall) == THREAD_RFLAGS_BEFORE_SYSCALL);
static_assert(offsetof(thread, teb) == THREAD_TEB);
