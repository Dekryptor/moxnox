module;

#include <span>
#include <variant>
#include <string_view>
#include <optional>
#include <functional>
#include "cpu_block.h"
import objects;
#include "spinlock.h"
#include "thread.h"
import page_cache;
import rwsem;
import rbtree;
#include "section.h"
#include "process.h"
#include "scheduler.h"

export module thread;

import print;
import mem;
import config;
import list;
import bsod;
import apic;

using namespace std;

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

static const uint16_t kernel_CS = 0x8;
static const uint16_t kernel_DS = 0x10;

export uint16_t last_tid; // FIXME

// process.cpp
extern section* ntdll;

export thread* get_current_thread() {
    disable_preempt();

    auto t = get_cpu_block()->current_thread;

    enable_preempt();

    return t;
}

export void thread_died(thread& t, bool& last_thread, bool remove_from_proc) {
    t.status = thread_status::dead;
    t.signalled = true;

    wake_up_waiters(t.waiters);

    remove_entry_list(t.le);
    insert_tail_list(thread_list_dead, t.le);

    if (t.proc && remove_from_proc) {
        lock_guard lock(t.proc->threads_lock);

        remove_entry_list(t.process_le);

        last_thread = is_list_empty(t.proc->threads);
    }
}

static void terminate_thread2(thread& t, bool& last_thread) {
    if (t.status == thread_status::dying || t.status == thread_status::dead)
        return;

    if (t.kernel_mode) {
        if (t.wait_interruptible) {
            switch (t.status) {
                case thread_status::waiting: {
                    remove_entry_list(t.wait_obj.wait_le);

                    if (t.wait_timeout != 0)
                        remove_entry_list(t.timeout_le);
                    break;
                }

                case thread_status::waiting_multiple: {
                    auto& block = *t.multwait;

                    for (unsigned int i = 0; i < block.num_entries; i++) {
                        if (!block.entries[i].done)
                            remove_entry_list(block.entries[i].wait_le);
                    }

                    if (t.wait_timeout != 0)
                        remove_entry_list(t.timeout_le);

                    t.multwait = nullptr;
                    break;
                }

                default:
                    break;
            }
        }

        t.status = thread_status::dying;
    } else { // user mode
        if (t.current_cpu == 0xffffffff)
            thread_died(t, last_thread, true);
        else {
            t.status = thread_status::dying;
            // FIXME - send yield ISR
        }
    }
}

export void terminate_thread(thread& t) {
    bool thread_is_dead, last_thread = false;

    t.acquire();

    __asm("cli");
    acquire_sync_lock();

    terminate_thread2(t, last_thread);

    thread_is_dead = t.status == thread_status::dead;

    release_sync_lock();
    __asm("sti");

    if (!t.proc || !thread_is_dead) {
        t.release();
        return;
    }

    if (last_thread) {
        t.proc->exit_status = t.exit_status;
        t.proc->terminate();
    } else {
        size_t region_size = 0;

        t.proc->free_vm((void**)&t.teb, &region_size, MEM_RELEASE);
        // FIXME - free userspace stack
    }

    t.release();
}

extern "C++"
thread::~thread() {
    if (kernel_stack) {
        auto va = (uintptr_t)kernel_stack + (KERNEL_GUARD_PAGES << PAGE_SHIFT);
        for (unsigned int i = 0; i < KERNEL_STACK_PAGES; i++) {
            auto pa = va_to_pa_lookup(va);

            free_folio(pa);
            unmap_page(va);

            va += PAGE_SIZE;
        }

        free_va((uintptr_t)kernel_stack, KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES);
    }

    if (proc)
        proc->release();
}

extern "C"
NTSTATUS NtTerminateThread(HANDLE thread_handle, NTSTATUS exit_status) {
    object_ref obj;

    if (thread_handle == NtCurrentThread()) {
        auto* t = get_current_thread();

        t->acquire();

        obj = object_ref{*t};
    } else {
        auto obj = deref_handle(thread_handle);
        if (!obj)
            return nt_status::INVALID_HANDLE;

        if (obj.get().type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights
    }

    auto& t = static_cast<thread&>(obj.get());

    t.exit_status = exit_status;
    terminate_thread(t);

    return nt_status::SUCCESS;
}

extern "C"
NTSTATUS NtCreateThread(HANDLE* thread_handle, ACCESS_MASK desired_access,
                        const OBJECT_ATTRIBUTES* object_attributes, HANDLE process_handle,
                        CLIENT_ID* client_id, const CONTEXT* thread_context,
                        const INITIAL_TEB* initial_teb, bool create_suspended) {
    object_ref objrw;
    process* p;

    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();

        objrw = object_ref{*p};
    } else {
        objrw = deref_handle(process_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        p = static_cast<process*>(&obj);
    }

    return p->create_thread(*thread_handle, desired_access, object_attributes, *client_id,
                            *thread_context, *initial_teb, create_suspended);
}

extern "C"
NTSTATUS NtResumeThread(HANDLE thread_handle, uint32_t* suspend_count) {
    object_ref objrw;
    thread* t;
    uint32_t count;

    if (thread_handle == NtCurrentThread()) {
        t = get_current_thread();
        t->acquire();

        objrw = object_ref{*t};
    } else {
        objrw = deref_handle(thread_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        t = static_cast<thread*>(&obj);
    }

    count = t->suspend_count.fetch_sub(1);

    if (suspend_count)
        *suspend_count = count;

    return nt_status::SUCCESS;
}

extern "C"
NTSTATUS NtQueryInformationThread(HANDLE thread_handle, enum THREAD_INFORMATION_CLASS thread_information_class,
                                  void* thread_information, uint32_t thread_information_length,
                                  uint32_t* return_length) {
    print("FIXME - NtQueryInformationThread\n");

    auto objrw = deref_handle(thread_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::thread)
        return nt_status::OBJECT_TYPE_MISMATCH;

    // FIXME - check access rights

    auto& t = static_cast<thread&>(obj);

    return t.query_information(thread_information_class, thread_information,
                               thread_information_length, return_length);
}

extern "C++"
NTSTATUS thread::query_information(enum THREAD_INFORMATION_CLASS /*thread_information_class*/,
                                   void* /*thread_information*/, uint32_t /*thread_information_length*/,
                                   uint32_t* /*return_length*/) {
    // FIXME

    return nt_status::NOT_IMPLEMENTED;
}

static void inline disable_interrupts2() {
    __asm __volatile("cli");
}

static void kernel_thread_start(void (*func)(void*, void*), void* param1, void* param2) {
    func(param1, param2);

    disable_interrupts2();

    auto& cb = *get_cpu_block();
    auto& t = *cb.current_thread;

    acquire_sync_lock();

    t.status = thread_status::dying;

    __asm __volatile ( // doesn't return, releases sync_lock
        "mov rsp, gs:%0\n"
        "jmp scheduler\n"
        :
        : "i" (GS_SCHEDULER_STACK)
    );
}

export object_ref add_kernel_thread(void (*func)(void*, void*), void* param1, void* param2) {
    uint16_t tid;

    auto tptr = new (align_val_t(16)) thread;
    if (!tptr) {
        print("new failed\n");
        return {};
    }

    auto& t = *tptr;

    // FIXME - spinlock for last_tid (or use atomics)
    if (last_tid == 0xffff) { // FIXME - check for lowest unused number
        print("Thread limit reached.\n");
        delete &t;
        return {};
    }

    last_tid++;
    tid = last_tid;

    t.tid = tid;

    t.kernel_stack = allocate_stack();
    if (!t.kernel_stack) {
        print("allocate_stack failed\n");
        delete &t;
        return {};
    }

    memset(&t.context, 0, sizeof(t.context));
    t.context.Rip = (uintptr_t)kernel_thread_start;
    t.context.Rcx = (uintptr_t)func;
    t.context.Rdx = (uintptr_t)param1;
    t.context.R8 = (uintptr_t)param2;

    // 0x20 because of MS ABI, minus 8 for return address
    t.context.Rsp = (uintptr_t)((uint8_t*)t.kernel_stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT) - 0x28);
    *(uint64_t*)t.context.Rsp = 0;

    t.context.EFlags = 0x202; // IF, reserved flag
    t.context.SegCs = kernel_CS;
    t.context.SegDs = kernel_DS;
    t.context.SegEs = kernel_DS;
    t.context.SegFs = kernel_DS;
    t.context.SegGs = kernel_DS;
    t.context.SegSs = kernel_DS;
    t.kernel_mode = true;
    t.teb = nullptr;

    t.status = thread_status::runnable;

    {
        sync_lock sl;
        lock_guard lock(sl);
        insert_tail_list(thread_list, t.le);
    }

    t.acquire(); // for active thread list

    return t;
}

export object_ref add_kernel_thread(void (*func)(void*), void* param) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
    return add_kernel_thread((void(*)(void*, void*))func, param, nullptr);
#pragma GCC diagnostic pop
}

export object_ref add_kernel_thread(void (*func)()) {
    return add_kernel_thread((void(*)(void*, void*))func, nullptr, nullptr);
}

static void inline enable_interrupts2() {
    __asm __volatile("sti");
}

extern "C"
NTSTATUS NtSuspendThread(HANDLE thread_handle, uint32_t* previous_suspend_count) {
    object_ref objrw;
    thread* t;

    if (thread_handle == NtCurrentThread()) {
        t = get_current_thread();
        t->acquire();

        objrw = object_ref{*t};
    } else {
        objrw = deref_handle(thread_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        t = static_cast<thread*>(&obj);
    }

    uint32_t count;

    // FIXME - send yield IPI if thread is currently running

    count = t->suspend_count.fetch_add(1);

    if (previous_suspend_count)
        *previous_suspend_count = count;

    disable_interrupts2();
    acquire_sync_lock();

    // break interruptible waits

    if (t->status == thread_status::waiting && t->wait_interruptible) {
        t->status = thread_status::runnable;
        remove_entry_list(t->wait_obj.wait_le);

        if (t->wait_timeout != 0)
            remove_entry_list(t->timeout_le);
    } else if (t->status == thread_status::waiting_multiple && t->wait_interruptible) {
        auto& block = *t->multwait;

        for (unsigned int i = 0; i < block.num_entries; i++) {
            if (!block.entries[i].done)
                remove_entry_list(block.entries[i].wait_le);
        }

        if (t->wait_timeout != 0)
            remove_entry_list(t->timeout_le);

        t->multwait = nullptr;
        t->status = thread_status::runnable;
    }

    // wait for thread to stop running

    if (t->status != thread_status::running && !t->kernel_mode) {
        release_sync_lock();
        enable_interrupts2();
        return nt_status::SUCCESS;
    }

    auto& this_thread = *get_cpu_block()->current_thread;

    insert_tail_list(t->suspend_waiters, this_thread.wait_obj.wait_le);

    set_current_thread_waiting();

    return nt_status::SUCCESS;
}

extern "C"
NTSTATUS NtGetContextThread(HANDLE thread_handle, CONTEXT* context) {
    object_ref objrw;
    thread* t;

    if (thread_handle == NtCurrentThread()) {
        t = get_current_thread();
        t->acquire();

        objrw = object_ref{*t};
    } else {
        objrw = deref_handle(thread_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        t = static_cast<thread*>(&obj);
    }

    {
        sync_lock sl;
        lock_guard lg(sl);

        if (t->kernel_mode)
            memcpy(context, &t->syscall_context, sizeof(CONTEXT));
        else
            memcpy(context, &t->context, sizeof(CONTEXT));
    }

    return nt_status::SUCCESS;
}

extern "C"
NTSTATUS NtSetContextThread(HANDLE thread_handle, const CONTEXT* context) {
    object_ref objrw;
    thread* t;

    if (thread_handle == NtCurrentThread()) {
        t = get_current_thread();
        t->acquire();

        objrw = object_ref{*t};
    } else {
        objrw = deref_handle(thread_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        t = static_cast<thread*>(&obj);
    }

    // FIXME - look at ContextFlags to determine what to copy

    // if non-canonical, iretq will cause a kernel-mode GPF
    if (context->Rip >> 48 != 0x0000 && context->Rip >> 48 != 0xffff)
        return nt_status::INVALID_PARAMETER;

    {
        sync_lock sl;
        lock_guard lg(sl);

        if (t->kernel_mode)
            memcpy(&t->syscall_context, context, sizeof(CONTEXT));
        else
            memcpy(&t->context, context, sizeof(CONTEXT));
    }

    return nt_status::SUCCESS;
}
