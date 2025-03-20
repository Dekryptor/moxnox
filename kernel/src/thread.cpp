#include "kernel.h"
#include "cpu_block.h"
import objects;
#include "handle.h"
#include "thread.h"
#include "section.h"
#include "process.h"
import rbtree;
import event;
import pipe;
import timer;
#include "visit_object.h"
#include <ntdll.h>

import print;
import mem;
import config;
import list;
import interrupts;

using namespace std;

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
    unsigned int release_count = 0;

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

        auto le = thread_list.next;

        while (le != &thread_list) {
            auto& t = containing_record<thread>(le);

            if (t.status == thread_status::waiting_object && t.waiting_for.has_value() && &t.waiting_for->get() == static_cast<object*>(this)) {
                t.status = thread_status::runnable;
                t.waiting_for = nullopt;
                release_count++;
            }

            le = le->next;
        }

        remove_entry_list(this->le);

        for (unsigned int i = 0; i < release_count; i++) {
            release();
        }

        if (current_cpu != 0xffffffff)
            insert_tail_list(thread_list_dead, this->le); // FIXME - release these in scheduler
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
            auto pa = va_to_pa(va);

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
        t->acquire();
    } else {
        auto objrw = deref_handle(thread_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw->get();

        if (obj.type != object_type::thread) {
            visit_obj(obj, [](auto&& o) {
                o.release();
            });

            return nt_status::OBJECT_TYPE_MISMATCH;
        }

        // FIXME - check access rights

        t = static_cast<thread*>(&objrw->get());
    }

    if (t == get_current_thread())
        t->release();

    t->kill(exit_status); // if this thread, doesn't return
    t->release();

    return nt_status::SUCCESS;
}

NTSTATUS process::create_thread(optional<reference_wrapper<handle_t>> thread_handle, ACCESS_MASK,
                                const OBJECT_ATTRIBUTES*, CLIENT_ID& client_id,
                                const CONTEXT& thread_context,
                                const INITIAL_TEB& initial_teb, bool create_suspended) {
    uint16_t tid;
    CONTEXT start_context;
    mn_image_info ntdll_info, img_info;

    // FIXME - desired_access
    // FIXME - object_attributes

    auto tptr = new (align_val_t(16)) thread;
    if (!tptr)
        return nt_status::INSUFFICIENT_RESOURCES;;

    auto& t = *tptr;

    // FIXME - spinlock for last_tid (or use atomics)
    if (last_tid == 0xffff) { // FIXME - check for lowest unused number
        print("Thread limit reached.\n");
        delete &t;
        return nt_status::INTERNAL_ERROR; // FIXME
    }

    last_tid++;
    tid = last_tid;

    t.tid = tid;

    t.teb = nullptr;
    size_t teb_size = sizeof(TEB);

    if (!success(allocate_vm((void**)&t.teb, 0, &teb_size, MEM_COMMIT | MEM_RESERVE, mapping_type::read_write))) {
        delete &t;
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    if (!populate_teb(t)) {
        delete &t;
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    memcpy(&start_context, &thread_context, sizeof(CONTEXT));
    start_context.EFlags |= 0x202; // IF, reserved flag
    start_context.SegCs = user_CS;
    start_context.SegSs = user_DS;
    start_context.SegEs = user_DS;
    start_context.SegFs = user_DS;
    start_context.SegGs = user_DS;

    auto ctx_addr = (uintptr_t)initial_teb.StackBase - sizeof(CONTEXT);

    if (!copy_to((void*)ctx_addr, &start_context, sizeof(start_context))) {
        print("process::copy_to failed\n");
        delete &t;
        return nt_status::NO_MEMORY;
    }

    ntdll_info.base = (void*)ntdll_addr;
    ntdll_info.offset_debug_info = ntdll->dwinfo.debug_info.offset;
    ntdll_info.size_debug_info = ntdll->dwinfo.debug_info.size;
    ntdll_info.offset_debug_abbrev = ntdll->dwinfo.debug_abbrev.offset;
    ntdll_info.size_debug_abbrev = ntdll->dwinfo.debug_abbrev.size;
    ntdll_info.offset_debug_rnglists = ntdll->dwinfo.debug_rnglists.offset;
    ntdll_info.size_debug_rnglists = ntdll->dwinfo.debug_rnglists.size;
    ntdll_info.offset_debug_str = ntdll->dwinfo.debug_str.offset;
    ntdll_info.size_debug_str = ntdll->dwinfo.debug_str.size;
    ntdll_info.offset_debug_addr = ntdll->dwinfo.debug_addr.offset;
    ntdll_info.size_debug_addr = ntdll->dwinfo.debug_addr.size;
    ntdll_info.offset_debug_str_offsets = ntdll->dwinfo.debug_str_offsets.offset;
    ntdll_info.size_debug_str_offsets = ntdll->dwinfo.debug_str_offsets.size;

    auto ntdll_info_addr = ctx_addr - sizeof(ntdll_info);

    if (!copy_to((void*)ntdll_info_addr, &ntdll_info, sizeof(ntdll_info))) {
        print("process::copy_to failed\n");
        delete &t;
        return nt_status::NO_MEMORY;
    }

    auto& sect = img->get();

    img_info.base = (void*)img_addr;
    img_info.offset_debug_info = sect.dwinfo.debug_info.offset;
    img_info.size_debug_info = sect.dwinfo.debug_info.size;
    img_info.offset_debug_abbrev = sect.dwinfo.debug_abbrev.offset;
    img_info.size_debug_abbrev = sect.dwinfo.debug_abbrev.size;
    img_info.offset_debug_rnglists = sect.dwinfo.debug_rnglists.offset;
    img_info.size_debug_rnglists = sect.dwinfo.debug_rnglists.size;
    img_info.offset_debug_str = sect.dwinfo.debug_str.offset;
    img_info.size_debug_str = sect.dwinfo.debug_str.size;
    img_info.offset_debug_addr = sect.dwinfo.debug_addr.offset;
    img_info.size_debug_addr = sect.dwinfo.debug_addr.size;
    img_info.offset_debug_str_offsets = sect.dwinfo.debug_str_offsets.offset;
    img_info.size_debug_str_offsets = sect.dwinfo.debug_str_offsets.size;

    auto img_info_addr = ntdll_info_addr - sizeof(img_info);

    if (!copy_to((void*)img_info_addr, &img_info, sizeof(img_info))) {
        print("process::copy_to failed\n");
        delete &t;
        return nt_status::NO_MEMORY;
    }

    memset(&t.context, 0, sizeof(CONTEXT));
    memset(&t.syscall_context, 0, sizeof(CONTEXT));

    t.context.EFlags |= 0x202; // IF, reserved flag
    t.context.SegCs = user_CS;
    t.context.SegSs = user_DS;
    t.context.SegEs = user_DS;
    t.context.SegFs = user_DS;
    t.context.SegGs = user_DS;
    t.context.Rip = ntdll_addr + mn_start_thread;
    t.context.Rcx = ntdll_info_addr;
    t.context.Rdx = img_info_addr;
    t.context.R8 = ctx_addr;
    t.context.Rsp = (uintptr_t)img_info_addr - 0x20; // 0x20 for MS ABI

    t.kernel_mode = false;

    t.suspend_count = create_suspended ? 1 : 0;
    t.status = thread_status::runnable;

    t.kernel_stack = allocate_stack();
    if (!t.kernel_stack) {
        delete &t;
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    t.syscall_stack = ((uint8_t*)t.kernel_stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT)) - 0x20; // 0x20 for MS ABI

    t.proc = this;
    this->acquire();

    {
        lock_guard lock(threads_lock);
        insert_tail_list(this->threads, t.process_le);
    }

    {
        sync_lock sl;
        lock_guard lock(sl);
        insert_tail_list(thread_list, t.le);
    }

    client_id.UniqueProcess = 0; // FIXME - PID
    client_id.UniqueThread = tid;

    // FIXME - copy initial_teb into TEB
    // FIXME - copy client_id into TEB

    if (thread_handle) {
        auto& p = *get_current_thread()->proc;

        handle_t h;

        auto status = p.add_handle(t, 0, true, h); // FIXME - access mask
        if (!success(status))
            return status;

        thread_handle->get() = h;
    }

    return nt_status::SUCCESS;
}

NTSTATUS NtCreateThread(HANDLE* thread_handle, ACCESS_MASK desired_access,
                        const OBJECT_ATTRIBUTES* object_attributes, HANDLE process_handle,
                        CLIENT_ID* client_id, const CONTEXT* thread_context,
                        const INITIAL_TEB* initial_teb, bool create_suspended) {
    process* p;

    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();
    } else {
        auto objrw = deref_handle(process_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw->get();

        if (obj.type != object_type::process) {
            visit_obj(obj, [](auto&& o) {
                o.release();
            });

            return nt_status::OBJECT_TYPE_MISMATCH;
        }

        // FIXME - check access rights

        p = static_cast<process*>(&objrw->get());
    }

    auto ret = p->create_thread(*thread_handle, desired_access, object_attributes, *client_id,
                                *thread_context, *initial_teb, create_suspended);

    p->release();

    return ret;
}

NTSTATUS NtResumeThread(HANDLE thread_handle, uint32_t* suspend_count) {
    thread* t;

    if (thread_handle == NtCurrentThread()) {
        t = get_current_thread();
        t->acquire();
    } else {
        auto objrw = deref_handle(thread_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw->get();

        if (obj.type != object_type::thread) {
            visit_obj(obj, [](auto&& o) {
                o.release();
            });

            return nt_status::OBJECT_TYPE_MISMATCH;
        }

        // FIXME - check access rights

        t = static_cast<thread*>(&objrw->get());
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

    t->release();

    return nt_status::SUCCESS;
}

NTSTATUS NtQueryInformationThread(HANDLE thread_handle, enum THREAD_INFORMATION_CLASS thread_information_class,
                                  void* thread_information, uint32_t thread_information_length,
                                  uint32_t* return_length) {
    print("FIXME - NtQueryInformationThread\n");

    auto objrw = deref_handle2(thread_handle);
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

    optional<reference_wrapper<object>> waiting_for2;

    {
        sync_lock sl;
        lock_guard l(sl);

        if (status == thread_status::waiting_object && wait_interruptible) {
            status = thread_status::runnable;
            swap(waiting_for, waiting_for2);
        }
    }

    if (waiting_for2.has_value()) {
        visit_obj(*waiting_for2, [](auto&& o) {
            o.release();
        });
    }

    // FIXME - send IPI to service signal, if currently running on a CPU in userspace?
}
