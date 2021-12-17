#pragma once

#include <cstdint>
#include "list.h"

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
    // FIXME - FXSAVE area
    uint64_t rflags;
    uint64_t cs;
    uint64_t ss;
    // FIXME - other segments
} amd64_context;

static_assert(offsetof(amd64_context, rax) == 0x00);
static_assert(offsetof(amd64_context, rcx) == 0x08);
static_assert(offsetof(amd64_context, rdx) == 0x10);
static_assert(offsetof(amd64_context, rbx) == 0x18);
static_assert(offsetof(amd64_context, rsp) == 0x20);
static_assert(offsetof(amd64_context, rbp) == 0x28);
static_assert(offsetof(amd64_context, rsi) == 0x30);
static_assert(offsetof(amd64_context, rdi) == 0x38);
static_assert(offsetof(amd64_context, r8) == 0x40);
static_assert(offsetof(amd64_context, r9) == 0x48);
static_assert(offsetof(amd64_context, r10) == 0x50);
static_assert(offsetof(amd64_context, r11) == 0x58);
static_assert(offsetof(amd64_context, r12) == 0x60);
static_assert(offsetof(amd64_context, r13) == 0x68);
static_assert(offsetof(amd64_context, r14) == 0x70);
static_assert(offsetof(amd64_context, r15) == 0x78);
static_assert(offsetof(amd64_context, rip) == 0x80);
static_assert(offsetof(amd64_context, rflags) == 0x88);
static_assert(offsetof(amd64_context, cs) == 0x90);
static_assert(offsetof(amd64_context, ss) == 0x98);

enum class thread_status {
    runnable,
    running,
    waiting,
    dead
};

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
};

static_assert(is_sync_object<thread>);
