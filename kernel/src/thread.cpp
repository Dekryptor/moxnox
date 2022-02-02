#include "kernel.h"
#include "cpu_block.h"
#include "objects.h"

using namespace std;

thread* get_current_thread() {
    __asm __volatile("cli");
    auto t = get_cpu_block()->current_thread;
    __asm __volatile("sti");

    return t;
}

void thread::terminate() {
    unsigned int release_count = 0;

    status = thread_status::dead;
    signalled = true;

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

    auto le = thread_list.next;

    while (le != &thread_list) {
        auto& t = containing_record<thread>(le);

        if (t.status == thread_status::waiting && t.waiting_for.has_value() && &t.waiting_for->get() == static_cast<object*>(this)) {
            t.status = thread_status::runnable;
            t.waiting_for = nullopt;
            release_count++;
        }

        le = le->next;
    }

    for (unsigned int i = 0; i < release_count; i++) {
        release();
    }

    yield(); // releases sync_lock
}

thread::~thread() {
    if (kernel_stack) {
        auto va = (uintptr_t)kernel_stack + (KERNEL_GUARD_PAGES << PAGE_SHIFT);
        for (unsigned int i = 0; i < KERNEL_STACK_PAGES; i++) {
            auto pa = va_to_pa(va);

            free_page(pa);
            unmap_page(va);

            va += PAGE_SIZE;
        }

        free_va((uintptr_t)kernel_stack, KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES);
    }

    if (proc)
        proc->release();
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

    if (t != get_current_thread()) {
        // FIXME
        print("FIXME - terminating other threads\n");
        t->release();
        return nt_status::NOT_IMPLEMENTED;
    }

    t->exit_status = exit_status;

    t->release();
    t->terminate(); // doesn't return

    return nt_status::SUCCESS;
}

NTSTATUS process::create_thread(handle_t& thread_handle, ACCESS_MASK desired_access,
                                const OBJECT_ATTRIBUTES* object_attributes,
                                CLIENT_ID& client_id, const CONTEXT& thread_context,
                                const INITIAL_TEB& initial_teb, bool create_suspended) {
    print("FIXME - process::create_thread\n");

    return nt_status::NOT_IMPLEMENTED;
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
