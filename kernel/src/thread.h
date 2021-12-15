#pragma once

#include <cstdint>

typedef struct {
    // FIXME - segments
    // FIXME - EFLAGS
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
} amd64_context;

enum class thread_status {
    runnable,
    running,
    waiting
};

typedef struct _list_entry {
    struct _list_entry* prev;
    struct _list_entry* next;
} list_entry;

typedef struct {
    list_entry le;
    amd64_context context;
    enum thread_status status;
    uint32_t tid;
    void* stack;
} thread;
