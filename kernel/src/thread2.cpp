module;

#include <optional>
#include <mutex>
#include <new>
#include <signal.h>
#include "timer-asm.h"

module thread;

import print;
import mem;
import config;
import disable_interrupts;
import cpu_block;
import spinlock;
import handle;
import crt;
import process;
import scheduler;
import interrupts;

using std::lock_guard;
using std::nullopt;

#ifdef __x86_64__
static const uint16_t kernel_CS = 0x8;
static const uint16_t kernel_DS = 0x10;
#endif

static const int LAST_SIG = SIGTTOU;

thread* get_current_thread() {
    disable_preempt();

    auto t = get_cpu_block()->current_thread;

    enable_preempt();

    return t;
}

void thread_died(thread& t, bool* last_thread, bool remove_from_proc) {
    t.status = thread_status::dead;
    t.signalled = true;

    wake_up_waiters(t.waiters);

    remove_entry_list(t.le);

    if (t.proc && remove_from_proc) {
        lock_guard lock(t.proc->threads_lock);

        remove_entry_list(t.process_le);

        if (last_thread)
            *last_thread = is_list_empty(t.proc->threads);
    }

    t.release();
}

static void terminate_thread2(thread& t, bool* last_thread,
                              bool remove_from_proc) {
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

            if (t.current_cpu == 0xffffffff && (t.status == thread_status::waiting || t.status == thread_status::waiting_multiple)) {
                t.status = thread_status::dying;
                return;
            }
        } else if (t.status == thread_status::waiting || t.status == thread_status::waiting_multiple) {
            t.kill_on_wake = true;
            return;
        }

        t.status = thread_status::dying;
    } else { // user mode
        if (t.current_cpu == 0xffffffff)
            thread_died(t, last_thread, remove_from_proc);
        else {
            t.status = thread_status::dying;
            // FIXME - send yield ISR
        }
    }
}

void terminate_thread(thread& t, bool remove_from_proc) {
    bool thread_is_dead, last_thread = false;

    t.acquire();

    disable_interrupts();
    acquire_sync_lock();

    terminate_thread2(t, &last_thread, remove_from_proc);

    thread_is_dead = t.status == thread_status::dead;

    release_sync_lock();
    enable_interrupts();

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

thread::~thread() {
    if (kernel_stack) {
        auto va = (uintptr_t)kernel_stack + (KERNEL_GUARD_PAGES << PAGE_SHIFT);
        uint64_t pages_left = KERNEL_STACK_PAGES;

        while (pages_left > 0) {
            auto pa = va_to_pa_lookup(va);
            auto order = get_folio_order(pa);

            free_folio(pa);

            for (unsigned int i = 0; i < (unsigned int)(1 << order); i++) {
                unmap_page(va);
                va += PAGE_SIZE;
            }

            pages_left -= 1 << order;
        }

        free_va((uintptr_t)kernel_stack, KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES);
    }

    while (true) {
        pending_apc* p = nullptr;

        {
            lock_guard lock(apc_lock);

            if (!is_list_empty(pending_apcs)) {
                p = &containing_record<pending_apc>(pending_apcs.next);
                remove_entry_list(p->le);
            }
        }

        if (!p)
            break;

        delete p;
    }

    if (proc)
        proc->release();
}

object_ref deref_handle(handle_t h) {
    auto t = get_current_thread();

    if (!t->proc)
        return nullopt;

    return t->proc->deref_handle(h);
}

object_ref deref_handle(handle_t h, ACCESS_MASK& access_mask) {
    auto t = get_current_thread();

    if (!t->proc)
        return nullopt;

    return t->proc->deref_handle(h, access_mask);
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
    terminate_thread(t, true);

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
NTSTATUS NtQueryInformationThread(HANDLE thread_handle,
                                  THREAD_INFORMATION_CLASS info_class,
                                  void* buffer, uint32_t buffer_size,
                                  uint32_t* return_length) {
    object_ref obj;

    if (thread_handle == NtCurrentThread()) {
        auto* t = get_current_thread();

        t->acquire();

        obj = object_ref{*t};
    } else {
        obj = deref_handle(thread_handle);
        if (!obj)
            return nt_status::INVALID_HANDLE;

        if (obj.get().type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights
    }

    auto& t = static_cast<thread&>(obj.get());

    return t.query_information(info_class, span((uint8_t*)buffer, buffer_size),
                               *return_length);
}

nt_status thread::query_information(THREAD_INFORMATION_CLASS info_class,
                                    span<uint8_t> buffer, uint32_t& return_length) {
    switch (info_class) {
        using enum THREAD_INFORMATION_CLASS;

        case MoxnoxSignalsMask:
            return query_signals_mask(buffer, return_length);

        default:
            return nt_status::NOT_IMPLEMENTED;
    }
}

nt_status thread::query_signals_mask(span<uint8_t> buffer, uint32_t& return_length) {
    if (buffer.size() != sizeof(sigset_t))
        return nt_status::INTERNAL_ERROR;

    auto& set = *(sigset_t*)buffer.data();

    {
        lock_guard lock(proc->signals_lock);
        set = blocked_signals;
    }

    return_length = sizeof(sigset_t);

    return nt_status::SUCCESS;
}

static void kernel_thread_start(void (*func)(void*, void*), void* param1, void* param2) {
    func(param1, param2);

    disable_interrupts();

    auto& cb = *get_cpu_block();
    auto& t = *cb.current_thread;

    acquire_sync_lock();

    t.status = thread_status::dying;

#ifdef __aarch64__
    __asm __volatile ( // doesn't return, releases sync_lock
        "add x0, x18, #%0\n\t"
        "ldr x0, [x0]\n\t"
        "mov sp, x0\n\t"
        "b scheduler"
        :
        : "i" (offsetof(cpu_block, scheduler_stack))
    );
#else
    __asm __volatile ( // doesn't return, releases sync_lock
        "mov rsp, gs:%0\n"
        "jmp scheduler\n"
        :
        : "i" (offsetof(cpu_block, scheduler_stack))
    );
#endif
}

object_ref add_kernel_thread(void (*func)(void*, void*), void* param1, void* param2) {
    auto tptr = new thread;
    if (!tptr) {
        print("new failed\n");
        return {};
    }

    auto& t = *tptr;

    t.tid = next_tid.fetch_add(1);

    t.kernel_stack = allocate_stack();
    if (!t.kernel_stack) {
        print("allocate_stack failed\n");
        delete &t;
        return {};
    }

    memset(&t.context, 0, sizeof(t.context));

#ifdef __aarch64__
    t.context.Pc = (uintptr_t)kernel_thread_start;
    t.context.X0 = (uintptr_t)func;
    t.context.X1 = (uintptr_t)param1;
    t.context.X2 = (uintptr_t)param2;

    t.syscall_stack = (void*)((uint8_t*)t.kernel_stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT) - 0x10);

    t.context.Cpsr = 0x5; // EL1 with SP_EL1
#elifdef __x86_64__
    memset(&t.xsave, 0, XSAVE_SIZE);

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
#endif

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

    // FIXME - send yield IPI if thread is currently running?

    count = t->suspend_count.fetch_add(1);

    if (previous_suspend_count)
        *previous_suspend_count = count;

    return nt_status::SUCCESS;
}

extern "C"
NTSTATUS NtGetContextThread(HANDLE thread_handle, CONTEXT* context) {
#ifdef __aarch64__ // FIXME
    (void)thread_handle;
    (void)context;

    return nt_status::NOT_IMPLEMENTED;
#else
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

    // FIXME - wait until not in kernel mode

    {
        sync_lock sl;
        lock_guard lg(sl);

        if (t->kernel_mode)
            memcpy(context, &t->syscall_context, sizeof(CONTEXT));
        else
            memcpy(context, &t->context, sizeof(CONTEXT));
    }

    return nt_status::SUCCESS;
#endif
}

extern "C"
NTSTATUS NtSetContextThread(HANDLE thread_handle, const CONTEXT* context) {
#ifdef __aarch64__ // FIXME
    (void)thread_handle;
    (void)context;

    return nt_status::NOT_IMPLEMENTED;
#else
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
#endif
}

nt_status queue_apc(thread& t, PS_APC_ROUTINE apc_routine, void* apc_arg1,
                    void* apc_arg2, void* apc_arg3) {
    auto p = new pending_apc;
    if (!p) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    p->routine = apc_routine;
    p->arg1 = (uintptr_t)apc_arg1;
    p->arg2 = (uintptr_t)apc_arg2;
    p->arg3 = (uintptr_t)apc_arg3;

    {
        lock_guard lock(t.apc_lock);

        insert_tail_list(t.pending_apcs, p->le);
    }

    {
        sync_lock sl;
        lock_guard lock1(sl);
        lock_guard lock2(t.apc_lock);

        if (is_list_empty(t.pending_apcs))
            return nt_status::SUCCESS;

        if ((t.status == thread_status::waiting || t.status == thread_status::waiting_multiple) && t.wait_interruptible) {
            // break waits

            if (t.wait_timeout != 0)
                remove_entry_list(t.timeout_le);

            if (t.status == thread_status::waiting)
                remove_entry_list(t.wait_obj.wait_le);
            else if (t.status == thread_status::waiting_multiple) {
                auto& block = *t.multwait;

                for (unsigned int i = 0; i < block.num_entries; i++) {
                    if (!block.entries[i].done)
                        remove_entry_list(block.entries[i].wait_le);
                }

                t.multwait = nullptr;
            }

            t.status = thread_status::runnable;
        }
    }

    return nt_status::SUCCESS;
}

extern "C"
NTSTATUS NtQueueApcThread(HANDLE thread_handle, PS_APC_ROUTINE apc_routine,
                          void* apc_arg1, void* apc_arg2, void* apc_arg3) {
    object_ref obj;

    if (thread_handle == NtCurrentThread()) {
        auto* t = get_current_thread();

        t->acquire();

        obj = object_ref{*t};
    } else {
        obj = deref_handle(thread_handle);
        if (!obj)
            return nt_status::INVALID_HANDLE;

        if (obj.get().type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights
    }

    auto& t = static_cast<thread&>(obj.get());

    return queue_apc(t, apc_routine, apc_arg1, apc_arg2, apc_arg3);
}

nt_status thread::set_information(THREAD_INFORMATION_CLASS info_class,
                                  span<const uint8_t> buffer) {
    switch (info_class) {
        using enum THREAD_INFORMATION_CLASS;

        case MoxnoxSetPendingSignal:
            return set_pending_signal(buffer);

        case MoxnoxBlockSignals:
            return block_signals(buffer);

        case MoxnoxUnblockSignals:
            return unblock_signals(buffer);

        case MoxnoxSignalsMask:
            return set_signals_mask(buffer);

        default:
            return nt_status::NOT_IMPLEMENTED;
    }
}

nt_status thread::set_pending_signal(span<const uint8_t> buffer) {
    if (buffer.size() != sizeof(int))
        return nt_status::INVALID_PARAMETER;

    auto sig = *(int*)buffer.data();

    if (sig <= 0 || sig > LAST_SIG)
        return nt_status::INVALID_PARAMETER;

    // FIXME - SIGKILL and SIGSTOP aren't really signals

    auto mask = (uint32_t)1 << (sig - 1);

    {
        lock_guard lock(proc->signals_lock);

        if (pending_signals & mask) // already pending
            return nt_status::SUCCESS;

        if (blocked_signals & mask) {
            pending_signals |= mask;
            return nt_status::SUCCESS;
        }
    }

    // not blocked, not already pending

    return queue_apc(*this, (PS_APC_ROUTINE)(proc->ntdll_addr + mn_posix_signal),
                     (void*)(uintptr_t)sig, nullptr, nullptr);
}

nt_status thread::block_signals(span<const uint8_t> buffer) {
    if (buffer.size() != sizeof(sigset_t))
        return nt_status::INVALID_PARAMETER;

    auto set = *(sigset_t*)buffer.data();

    set &= ((sigset_t)1 << LAST_SIG) - 1;

    {
        lock_guard lock(proc->signals_lock);

        blocked_signals |= set;
    }

    return nt_status::SUCCESS;
}

nt_status thread::unblock_signals(span<const uint8_t> buffer) {
    nt_status status;

    if (buffer.size() != sizeof(sigset_t))
        return nt_status::INVALID_PARAMETER;

    auto set = *(sigset_t*)buffer.data();

    set &= ((sigset_t)1 << LAST_SIG) - 1;

    sigset_t to_dispatch;

    {
        lock_guard lock(proc->signals_lock);

        blocked_signals &= ~set;

        to_dispatch = pending_signals & ~blocked_signals;
        pending_signals &= ~to_dispatch;
    }

    if (to_dispatch != 0) {
        for (int i = 1; i <= LAST_SIG; i++) {
            if (to_dispatch & ((sigset_t)1 << (i - 1))) {
                status = queue_apc(*this, (PS_APC_ROUTINE)(proc->ntdll_addr + mn_posix_signal),
                                   (void*)(uintptr_t)i, nullptr, nullptr);
                if (!success(status))
                    return status;
            }
        }
    }

    return nt_status::SUCCESS;
}

nt_status thread::set_signals_mask(span<const uint8_t> buffer) {
    nt_status status;

    if (buffer.size() != sizeof(sigset_t))
        return nt_status::INVALID_PARAMETER;

    auto set = *(sigset_t*)buffer.data();

    set &= ((sigset_t)1 << LAST_SIG) - 1;

    sigset_t to_dispatch;

    {
        lock_guard lock(proc->signals_lock);

        blocked_signals = set;

        to_dispatch = pending_signals & ~blocked_signals;
        pending_signals &= ~to_dispatch;
    }

    if (to_dispatch != 0) {
        for (int i = 1; i <= LAST_SIG; i++) {
            if (to_dispatch & ((sigset_t)1 << (i - 1))) {
                status = queue_apc(*this, (PS_APC_ROUTINE)(proc->ntdll_addr + mn_posix_signal),
                                   (void*)(uintptr_t)i, nullptr, nullptr);
                if (!success(status))
                    return status;
            }
        }
    }

    return nt_status::SUCCESS;
}

extern "C"
NTSTATUS NtSetInformationThread(HANDLE thread_handle, THREAD_INFORMATION_CLASS info_class,
                                const void* buffer, uint32_t buffer_size) {
    object_ref obj;

    if (thread_handle == NtCurrentThread()) {
        auto* t = get_current_thread();

        t->acquire();

        obj = object_ref{*t};
    } else {
        obj = deref_handle(thread_handle);
        if (!obj)
            return nt_status::INVALID_HANDLE;

        if (obj.get().type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights
    }

    auto& t = static_cast<thread&>(obj.get());

    return t.set_information(info_class, span((uint8_t*)buffer, buffer_size));
}
