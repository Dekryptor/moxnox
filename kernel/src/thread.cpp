#include "kernel.h"
#include "cpu_block.h"
#include "objects.h"

using namespace std;

extern uint16_t last_tid; // FIXME

thread* get_current_thread() {
    __asm __volatile("cli");
    auto t = get_cpu_block()->current_thread;
    __asm __volatile("sti");

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

    status = thread_status::dead;
    signalled = true;

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

    if (this != get_current_thread()) {
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

    t->exit_status = exit_status;

    if (t != get_current_thread()) {
        {
            sync_lock sl;
            lock_guard lock(sl);

            // FIXME - send IPI if currently running, to force it to yield

            t->status = thread_status::dead;
        }

        t->terminate();
        t->release();
    } else {
        t->release();
        t->terminate(); // doesn't return
    }

    return nt_status::SUCCESS;
}

NTSTATUS process::create_thread(optional<reference_wrapper<handle_t>> thread_handle, ACCESS_MASK,
                                const OBJECT_ATTRIBUTES*, CLIENT_ID& client_id,
                                const CONTEXT& thread_context,
                                const INITIAL_TEB& initial_teb, bool create_suspended) {
    uint16_t tid;

    // FIXME - desired_access
    // FIXME - object_attributes

    auto tptr = new thread;
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

    memcpy(&t.context, &thread_context, sizeof(CONTEXT));
    t.context.EFlags |= 0x202; // IF, reserved flag
    t.context.SegCs = user_CS;
    t.context.SegSs = user_DS;
    t.context.SegEs = user_DS;
    t.context.SegFs = user_DS;
    t.context.SegGs = user_DS;
    t.context.SegSs = user_DS;
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

        auto h = p.add_handle(t, 0); // FIXME - access mask

        if (!h)
            return nt_status::INTERNAL_ERROR;

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
