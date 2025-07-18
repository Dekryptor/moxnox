#pragma once

#include <cstdint>
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
    waiting_multiple,
    dead
};

class process;

enum class waiter_object_type {
    thread,
    multiple
};

class waiter_object {
public:
    waiter_object(enum waiter_object_type type) : type(type) { }

    list_entry wait_le;
    enum waiter_object_type type;
};

struct multiple_wait_block;

class waiter_object_multiple : public waiter_object {
public:
    waiter_object_multiple() : waiter_object(waiter_object_type::multiple) { }

    multiple_wait_block* block;
    bool done = false;
};

struct multiple_wait_block {
    thread* t;
    std::atomic<unsigned int> left;
    unsigned int index;
    unsigned int num_entries;
    waiter_object_multiple entries[0];
};

class thread : public sync_object {
public:
    thread() : sync_object(object_type::thread, false) { }
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
    NTSTATUS query_information(enum THREAD_INFORMATION_CLASS thread_information_class,
                               void* thread_information, uint32_t thread_information_length,
                               uint32_t* return_length);
    void send_signal(uint64_t code);

    list_entry le;
    CONTEXT context;
    CONTEXT syscall_context;
    enum thread_status status;
    uint32_t tid;
    void* kernel_stack = nullptr;
    process* proc = nullptr;
    uint64_t affinity = 0xffffffffffffffff;
    void* syscall_stack = nullptr;
    bool kernel_mode;
    enum nt_status exit_status = nt_status::PENDING;
    list_entry process_le;
    std::atomic<uint32_t> suspend_count = 0;
    TEB* teb;
    uint32_t current_cpu = 0xffffffff;
    uint64_t pending_signal = 0;
    bool wait_interruptible;
    waiter_object wait_obj{waiter_object_type::thread};
    list_entry timeout_le;
    uint64_t wait_timeout;
    multiple_wait_block* multwait;
} __attribute__ ((aligned (16)));

static_assert(is_sync_object<thread>);

static_assert(offsetof(thread, context) == THREAD_CONTEXT);
static_assert(offsetof(thread, syscall_context) == THREAD_SYSCALL_CONTEXT);
static_assert(offsetof(thread, proc) == THREAD_PROC);
static_assert(offsetof(thread, syscall_stack) == THREAD_SYSCALL_STACK);
static_assert(offsetof(thread, kernel_mode) == THREAD_KERNEL_MODE);
static_assert(offsetof(thread, suspend_count) == THREAD_SUSPEND_COUNT);
static_assert(offsetof(thread, teb) == THREAD_TEB);
static_assert(offsetof(thread, pending_signal) == THREAD_PENDING_SIGNAL);
static_assert(offsetof(thread, timeout_le) == THREAD_TIMEOUT_LE);
static_assert(offsetof(thread, wait_timeout) == THREAD_WAIT_TIMEOUT);
