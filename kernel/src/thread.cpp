#include "kernel.h"
#include "cpu_block.h"
import objects;
#include "handle.h"
#include "spinlock.h"
#include "thread.h"
import rbtree;
#include "inode.h"
#include "section.h"
#include "process.h"

import print;
import mem;
import config;
import list;
import interrupts;
import bsod;

using namespace std;

static const uint16_t kernel_CS = 0x8;
static const uint16_t kernel_DS = 0x10;

extern uint16_t last_tid; // FIXME

// process.cpp
extern section* ntdll;

thread* get_current_thread() {
    disable_preempt();

    auto t = get_cpu_block()->current_thread;

    enable_preempt();

    return t;
}

void thread::terminate() {
    if (proc) {
        bool last_thread;
        size_t region_size = 0;

        {
            lock_guard lock(proc->threads_lock);

            remove_entry_list(process_le);

            last_thread = is_list_empty(proc->threads);
        }

        if (last_thread) {
            proc->exit_status = exit_status;
            proc->terminate();
        } else {
            proc->free_vm((void**)&teb, &region_size, MEM_RELEASE);
            // FIXME - free userspace stack
        }
    }

    __asm("cli");
    acquire_sync_lock();

    if (status != thread_status::dead) {
        status = thread_status::dead;
        signalled = true;

        if (current_cpu != 0xffffffff)
            acquire();

        wake_up_waiters(waiters);

        remove_entry_list(this->le);

        if (current_cpu != 0xffffffff) {
            insert_tail_list(thread_list_dead, this->le); // FIXME - release these in scheduler
            release();
        }
    }

    if (this != get_cpu_block()->current_thread) {
        release_sync_lock();
        __asm("sti");
        return;
    }

    yield(); // releases sync_lock
}

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

void thread::kill(NTSTATUS exit_status) {
    this->exit_status = exit_status;

    if (this != get_current_thread()) {
        bool sent_ipi = false;

        {
            sync_lock sl;
            lock_guard lock(sl);

            if (status != thread_status::dead) {
                status = thread_status::dead;

                if (current_cpu != 0xffffffff) {
                    // send IPI if currently running, to force it to yield

                    auto icr1 = (volatile uint32_t*)&local_apic[0x300];
                    auto icr2 = (volatile uint32_t*)&local_apic[0x310];

                    *icr2 = current_cpu << 24;
                    *icr1 = 0x5000 | yield_interrupt;

                    while (*icr1 & 0x1000) { }

                    sent_ipi = true;
                }
            }
        }

        if (sent_ipi) { // wait for it to die
            while (true) {
                sync_lock sl;
                lock_guard lock(sl);

                if (current_cpu == 0xffffffff)
                    break;
            }
        }
    }

    terminate();
}

NTSTATUS NtTerminateThread(HANDLE thread_handle, NTSTATUS exit_status) {
    thread* t;

    if (thread_handle == NtCurrentThread()) {
        t = get_current_thread();

        t->kill(exit_status); // if this thread, doesn't return
    } else {
        auto objrw = deref_handle(thread_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        t = static_cast<thread*>(&obj);

        if (t == get_current_thread())
            t->release();

        t->kill(exit_status); // if this thread, doesn't return
        t->release();
    }

    return nt_status::SUCCESS;
}

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

NTSTATUS NtResumeThread(HANDLE thread_handle, uint32_t* suspend_count) {
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

    {
        sync_lock sl;
        lock_guard lock(sl);

        count = t->suspend_count;

        if (count > 0)
            t->suspend_count--;
    }

    if (suspend_count)
        *suspend_count = count;

    return nt_status::SUCCESS;
}

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

NTSTATUS thread::query_information(enum THREAD_INFORMATION_CLASS /*thread_information_class*/,
                                   void* /*thread_information*/, uint32_t /*thread_information_length*/,
                                   uint32_t* /*return_length*/) {
    // FIXME

    return nt_status::NOT_IMPLEMENTED;
}

void thread::send_signal(uint64_t code) {
    // FIXME - should be able to have multiple pending signals
    // FIXME - EXCEPTION_RECORD

    pending_signal = code;

    // if currently doing an interruptible wait, wake up

    {
        sync_lock sl;
        lock_guard l(sl);

        if (status == thread_status::waiting && wait_interruptible) {
            status = thread_status::runnable;
            remove_entry_list(wait_le);

            if (wait_timeout != 0)
                remove_entry_list(timeout_le);
        }
    }

    // FIXME - send IPI to service signal, if currently running on a CPU in userspace?
}

static void kernel_thread_start(void (*func)(void*, void*), void* param1, void* param2) {
    func(param1, param2);

    get_current_thread()->terminate();
}

object_ref add_kernel_thread(void (*func)(void*, void*), void* param1, void* param2) {
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

object_ref add_kernel_thread(void (*func)(void*), void* param) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
    return add_kernel_thread((void(*)(void*, void*))func, param, nullptr);
#pragma GCC diagnostic pop
}

object_ref add_kernel_thread(void (*func)()) {
    return add_kernel_thread((void(*)(void*, void*))func, nullptr, nullptr);
}
