#pragma once

#include <nt.h>
#include <cstdint>
#include <atomic>
#include "context_offsets.h"

import list;
import object_ref;
import objects;

enum class thread_status {
    runnable,
    running,
    waiting,
    waiting_multiple,
    dead,
    dying
};

extern "C++" class process;

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

    list_entry le;
    CONTEXT context;
    CONTEXT syscall_context;
    enum thread_status status;
    uint32_t tid;
    void* kernel_stack = nullptr;
    process* proc = nullptr;
    uint64_t affinity = 0xffffffffffffffff;
    void* syscall_stack = nullptr;
#ifdef __aarch64__
    uint64_t user_stack;
#endif
    bool kernel_mode;
    enum nt_status exit_status = nt_status::PENDING;
    list_entry process_le;
    std::atomic<uint32_t> suspend_count = 0;
    TEB* teb;
    uint32_t current_cpu = 0xffffffff;
    bool wait_interruptible;
    waiter_object wait_obj{waiter_object_type::thread};
    list_entry timeout_le;
    uint64_t wait_timeout;
    multiple_wait_block* multwait;
} __attribute__ ((aligned (16)));

static_assert(is_sync_object<thread>);

static_assert(offsetof(thread, context) == THREAD_CONTEXT);
static_assert(offsetof(thread, syscall_stack) == THREAD_SYSCALL_STACK);

#ifdef __aarch64__
static_assert(offsetof(thread, user_stack) == THREAD_USER_STACK);
#else
static_assert(offsetof(thread, syscall_context) == THREAD_SYSCALL_CONTEXT);
static_assert(offsetof(thread, status) == THREAD_STATUS);
static_assert(offsetof(thread, proc) == THREAD_PROC);
static_assert(offsetof(thread, kernel_mode) == THREAD_KERNEL_MODE);
static_assert(offsetof(thread, suspend_count) == THREAD_SUSPEND_COUNT);
static_assert(offsetof(thread, teb) == THREAD_TEB);
static_assert(offsetof(thread, timeout_le) == THREAD_TIMEOUT_LE);
static_assert(offsetof(thread, wait_timeout) == THREAD_WAIT_TIMEOUT);

static_assert(sizeof(thread::status) == 4);
static_assert((unsigned int)thread_status::dead == ENUM_THREAD_STATUS_DEAD);
#endif

struct multiple_wait_block {
    thread* t;
    std::atomic<unsigned int> left;
    unsigned int index;
    unsigned int num_entries;
    waiter_object_multiple entries[0];
};
