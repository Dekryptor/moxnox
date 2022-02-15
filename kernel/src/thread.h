#pragma once

#include <cstdint>
#include "list.h"
#include "context_offsets.h"

static_assert(sizeof(AMD64_CONTEXT) == 0x4d0);

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
    void kill(NTSTATUS exit_status);

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
    uint32_t current_cpu = 0xffffffff;
} __attribute__ ((aligned (16)));

static_assert(is_sync_object<thread>);

static_assert(offsetof(thread, context) == THREAD_CONTEXT);
static_assert(offsetof(thread, proc) == THREAD_PROC);
static_assert(offsetof(thread, syscall_stack) == THREAD_SYSCALL_STACK);
static_assert(offsetof(thread, kernel_mode) == THREAD_KERNEL_MODE);
static_assert(offsetof(thread, rip_before_syscall) == THREAD_RIP_BEFORE_SYSCALL);
static_assert(offsetof(thread, rsp_before_syscall) == THREAD_RSP_BEFORE_SYSCALL);
static_assert(offsetof(thread, rflags_before_syscall) == THREAD_RFLAGS_BEFORE_SYSCALL);
static_assert(offsetof(thread, teb) == THREAD_TEB);
