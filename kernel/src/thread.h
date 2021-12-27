#pragma once

#include <cstdint>
#include "list.h"
#include "context_offsets.h"

// FIXME - make compatible with NT version
typedef struct _amd64_context {
    // FIXME - debug registers
    uint64_t rax;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rbx;
    uint64_t rsp;
    uint64_t rbp;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;
    uint64_t rflags;
    uint16_t cs;
    uint16_t ds;
    uint16_t es;
    uint16_t fs;
    uint16_t gs;
    uint16_t ss;
    char fxsave_area[512]  __attribute__ ((aligned (16)));
} amd64_context __attribute__ ((aligned (16)));

static_assert(offsetof(amd64_context, rax) == CONTEXT_RAX);
static_assert(offsetof(amd64_context, rcx) == CONTEXT_RCX);
static_assert(offsetof(amd64_context, rdx) == CONTEXT_RDX);
static_assert(offsetof(amd64_context, rbx) == CONTEXT_RBX);
static_assert(offsetof(amd64_context, rsp) == CONTEXT_RSP);
static_assert(offsetof(amd64_context, rbp) == CONTEXT_RBP);
static_assert(offsetof(amd64_context, rsi) == CONTEXT_RSI);
static_assert(offsetof(amd64_context, rdi) == CONTEXT_RDI);
static_assert(offsetof(amd64_context, r8) == CONTEXT_R8);
static_assert(offsetof(amd64_context, r9) == CONTEXT_R9);
static_assert(offsetof(amd64_context, r10) == CONTEXT_R10);
static_assert(offsetof(amd64_context, r11) == CONTEXT_R11);
static_assert(offsetof(amd64_context, r12) == CONTEXT_R12);
static_assert(offsetof(amd64_context, r13) == CONTEXT_R13);
static_assert(offsetof(amd64_context, r14) == CONTEXT_R14);
static_assert(offsetof(amd64_context, r15) == CONTEXT_R15);
static_assert(offsetof(amd64_context, rip) == CONTEXT_RIP);
static_assert(offsetof(amd64_context, rflags) == CONTEXT_RFLAGS);
static_assert(offsetof(amd64_context, cs) == CONTEXT_CS);
static_assert(offsetof(amd64_context, ds) == CONTEXT_DS);
static_assert(offsetof(amd64_context, es) == CONTEXT_ES);
static_assert(offsetof(amd64_context, fs) == CONTEXT_FS);
static_assert(offsetof(amd64_context, gs) == CONTEXT_GS);
static_assert(offsetof(amd64_context, ss) == CONTEXT_SS);
static_assert(offsetof(amd64_context, fxsave_area) == CONTEXT_FXSAVE); // must be aligned on 16-byte boundary

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

    list_entry le;
    bool signalled = false;
    amd64_context context;
    enum thread_status status;
    uint32_t tid;
    void* stack = nullptr;
    std::optional<std::reference_wrapper<object>> waiting_for; // FIXME - multiple objects
    process* proc = nullptr;
};

static_assert(is_sync_object<thread>);
