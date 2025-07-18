#include "kernel.h"
#include "cpu_block.h"
import objects;
#include "thread.h"
#include "process.h"
#include "tick_rate.h"

import print;
import bsod;
import mem;
import config;
import list;
import interrupts;
import visit;

using namespace std;

uint16_t last_tid = 0; // FIXME - too small?

static const uint64_t time_slice = 50; // ticks

list_entry thread_list = { &thread_list, &thread_list };
list_entry thread_list_dead = { &thread_list_dead, &thread_list_dead };
list_entry thread_list_timeout = { &thread_list_timeout, &thread_list_timeout };

static atomic<uint64_t> cpu_block_va = 0xfffffa3000000000;

volatile uint64_t sync_lock_val = 0;

#ifdef DEBUG
uint32_t sync_lock_cpu = 0xffffffff;
#endif

static thread* pick_runnable_thread(thread* current) {
    list_entry* le;
    auto cb = get_cpu_block();
    uint64_t mask = 1 << cb->cpu_no; // FIXME - is the local APIC ID guaranteed to start at 0?

    // FIXME - proper scheduling algorithm (currently just loops through runnable threads)

    auto consider = [&](thread& t) -> thread* {
        if (t.status == thread_status::runnable &&
            t.affinity & mask &&
            (t.suspend_count == 0 || t.kernel_mode)) {
            return &t;
        }

        return nullptr;
    };

    if (current) { // loop through list from current thread
        le = current->le.next;
        while (le != &thread_list) {
            auto& t = containing_record<thread>(le);

            auto ret = consider(t);
            if (ret)
                return ret;

            le = le->next;
        }
    }

    le = thread_list.next;
    while (le != &thread_list) {
        auto& t = containing_record<thread>(le);

        auto ret = consider(t);
        if (ret)
            return ret;

        if (&t == current)
            break;

        le = le->next;
    }

    return nullptr;
}

static bool handle_signal() {
    auto& cb = *get_cpu_block();
    auto& t = *cb.current_thread;
    auto& p = *t.proc;

    uintptr_t us_context = cb.thread_context.Rsp - sizeof(CONTEXT);

    // FIXME - terminate thread if copy fails

    if (!p.copy_to((void*)us_context, &cb.thread_context, sizeof(CONTEXT)))
        return false;

    EXCEPTION_RECORD er;

    er.ExceptionCode = (uint32_t)t.pending_signal;
    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)cb.thread_context.Rip;
    er.NumberParameters = 0;

    size_t er_size = offsetof(EXCEPTION_RECORD, ExceptionInformation) + (sizeof(uintptr_t) * er.NumberParameters);
    uintptr_t us_er = us_context - er_size;

    if (!p.copy_to((void*)us_er, &er, er_size))
        return false;

    cb.thread_context.Rip = p.ntdll_addr + mn_signal;
    cb.thread_context.Rcx = us_er;
    cb.thread_context.Rdx = us_context;
    cb.thread_context.Rsp -= sizeof(CONTEXT);
    cb.thread_context.Rsp -= er_size;

    return true;
}

extern "C"
void scheduler() {
#ifdef DEBUG
    uint64_t rflags = 0;

    __asm __volatile(
        "pushf\n"
        "pop rax\n"
        "mov %0, rax\n"
        :
        : "m" (rflags)
        : "rax"
    );

    if (rflags & 0x200) // IF flag set
        bsod("calling scheduler with interrupts enabled");
#endif

    auto& cb = *get_cpu_block();

    // scheduler is called with sync_lock acquired

    if (cb.current_thread) {
        bool dead = false;

        memcpy(&cb.current_thread->context, &cb.thread_context, sizeof(CONTEXT));
        cb.current_thread->current_cpu = 0xffffffff;

        if (cb.current_thread->proc)
            cb.current_thread->proc->running_cpus &= ~((uint64_t)1 << cb.cpu_no);

        if (cb.current_thread->status == thread_status::dead) {
            remove_entry_list(cb.current_thread->le);
            dead = true;
        } else if (cb.current_thread->status == thread_status::running)
            cb.current_thread->status = thread_status::runnable;
        else
            cb.current_thread = nullptr;

        release_sync_lock();

        if (dead) {
            cb.current_thread->release();
            cb.current_thread = nullptr;
        }
    } else
        release_sync_lock();

    do {
        acquire_sync_lock();

        if (cb.current_thread && cb.current_thread->status == thread_status::dead) {
            cb.current_thread->release();
            cb.current_thread = nullptr;
        }

        cb.current_thread = pick_runnable_thread(cb.current_thread);

        if (cb.current_thread) {
            assert(cb.current_thread->status == thread_status::runnable);
            assert(cb.current_thread->current_cpu == 0xffffffff);
            cb.current_thread->status = thread_status::running;
            cb.current_thread->current_cpu = cb.cpu_no;

            if (cb.current_thread->proc)
                cb.current_thread->proc->running_cpus |= (uint64_t)1 << cb.cpu_no;
        }

        release_sync_lock();

        if (cb.current_thread) {
            memcpy(&cb.thread_context, &cb.current_thread->context, sizeof(CONTEXT));
            cb.time_slice_left = time_slice;

            if (!cb.current_thread->kernel_mode && cb.current_thread->pending_signal) {
                if (!handle_signal()) {
                    acquire_sync_lock();
                    cb.current_thread->status = thread_status::runnable;
                    cb.current_thread->current_cpu = 0xffffffff;

                    if (cb.current_thread->proc)
                        cb.current_thread->proc->running_cpus &= ~((uint64_t)1 << cb.cpu_no);

                    release_sync_lock();

                    cb.current_thread->release();
                    cb.current_thread = nullptr;
                } else
                    __asm("jmp scheduler_restore"); // iretq will re-enable interrupts
            } else
                __asm("jmp scheduler_restore"); // iretq will re-enable interrupts
        }

        __asm("sti; hlt; cli");
    } while (true);
}

nt_status wait_for_object(object& obj, bool interruptible,
                          optional<uint64_t> timeout) {
#ifdef DEBUG
    uint64_t cr8 = 0, rflags = 0;

    __asm __volatile(
        "mov rax, cr8\n"
        "mov %0, rax\n"
        :
        : "m" (cr8)
        : "rax"
    );

    if (cr8)
        bsod("calling wait_for_object when cr8 != 0");

    __asm __volatile(
        "pushf\n"
        "pop rax\n"
        "mov %0, rax\n"
        :
        : "m" (rflags)
        : "rax"
    );

    if (!(rflags & 0x200)) // IF flag not set
        bsod("calling wait_for_object with interrupts disabled");
#endif
    auto current_thread = get_current_thread();

    __asm("cli");
    acquire_sync_lock();

    if (current_thread->status != thread_status::dead) {
        if (!object_is_sync_object(obj)) {
            release_sync_lock();
            __asm("sti");
            // FIXME - exception
            print("Not a sync object\n");
            return nt_status::INVALID_PARAMETER;
        }

        auto& so = object_get_sync_object(obj);

        if (so.signalled) {
            object_on_wake(obj);
            release_sync_lock();
            __asm("sti");

            return nt_status::SUCCESS;
        }

        if (timeout.has_value() && *timeout == 0) {
            release_sync_lock();
            __asm("sti");
            return nt_status::TIMEOUT;
        }

        insert_tail_list(so.waiters, current_thread->wait_obj.wait_le);

        auto& cb = *get_cpu_block();

        current_thread->status = thread_status::waiting;
        current_thread->current_cpu = 0xffffffff;
        current_thread->wait_interruptible = interruptible;

        if (current_thread->proc)
            current_thread->proc->running_cpus &= ~((uint64_t)1 << cb.cpu_no);

        if (!timeout.has_value())
            current_thread->wait_timeout = 0;
        else {
            current_thread->wait_timeout = *timeout / (SYSTEM_TIME_RESOLUTION / TICK_RATE);

            if (*timeout % (SYSTEM_TIME_RESOLUTION / TICK_RATE))
                current_thread->wait_timeout++;

            insert_tail_list(thread_list_timeout, current_thread->timeout_le);
        }
    }

    yield(); // releases sync_lock

    if (interruptible && current_thread->pending_signal)
        return nt_status::ALERTED;

    if (timeout.has_value() && current_thread->wait_timeout == 0)
        return nt_status::TIMEOUT;

    return nt_status::SUCCESS;
}

unsigned int wake_up_waiters(list_entry& waiters) {
    unsigned int ret = 0;

    while (!is_list_empty(waiters)) {
        auto& wo = *(waiter_object*)((uint8_t*)waiters.next - offsetof(waiter_object, wait_le));

        remove_entry_list(wo.wait_le);

        switch (wo.type) {
            case waiter_object_type::thread: {
                auto& t = *(thread*)((uint8_t*)&wo - offsetof(thread, wait_obj));

                assert(t.status == thread_status::waiting);

                if (t.wait_timeout != 0)
                    remove_entry_list(t.timeout_le);

                t.status = thread_status::runnable;

                break;
            }

            case waiter_object_type::multiple: {
                auto& wom = static_cast<waiter_object_multiple&>(wo);
                auto& block = *wom.block;
                auto& t = *block.t;

                assert(t.status == thread_status::waiting_multiple);

                if (t.wait_timeout != 0)
                    remove_entry_list(t.timeout_le);

                wom.done = true;

                if (--block.left == 0) {
                    for (unsigned int i = 0; i < block.num_entries; i++) {
                        if (!block.entries[i].done)
                            remove_entry_list(block.entries[i].wait_le);
                        else if (&block.entries[i] == &wom)
                            t.multwait->index = i;
                    }

                    t.multwait = nullptr;
                    t.status = thread_status::runnable;
                }

                break;
            }
        }

        ret++;
    }

    return ret;
}

extern "C"
void wait_timeout_expired(thread* tptr) {
    // interrupts disabled and sync_lock held

    auto& t = *tptr;

    assert(t.status == thread_status::waiting || t.status == thread_status::waiting_multiple);

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

bool init_cpu_block(unsigned int cpu_no) {
    auto pages = bytes_to_pages(sizeof(cpu_block));
    auto va = cpu_block_va.fetch_add(pages << PAGE_SHIFT);
    auto& cb = *(cpu_block*)va;

    for (unsigned int i = 0; i < pages; i++) {
        auto pa = allocate_page();
        if (!pa) {
            print("allocate_page failed\n");
            return false;
        }

        if (!map_page(pa, va, mapping_type::read_write)) {
            print("map_page failed\n");
            return false;
        }

        va += PAGE_SIZE;
    }

    auto printk_scratch_pa = allocate_page();
    if (!printk_scratch_pa) {
        print("allocate_page failed\n");
        return false;
    }

    new (&cb) cpu_block;

    cb.self = &cb;
    cb.current_thread = nullptr;
    cb.cpu_no = cpu_no;
    cb.printk_scratch = (char*)phys_address(printk_scratch_pa);

    auto stack = allocate_stack();
    if (!stack)
        return false;

    cb.scheduler_stack = ((uint8_t*)stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT)) - 0x20; // 0x20 for MS ABI

    __asm __volatile (
        "mov rax, cr3\n"
        : "=a" (cb.kernel_cr3)
    );

    __asm __volatile(
        "mov ecx, 0xc0000101\n\t"
        "mov rax, %0\n\t"
        "mov rdx, %0\n\t"
        "shr rdx, 32\n\t"
        "wrmsr\n\t"
        :
        : "m" (cb)
        : "ecx", "rax", "rdx"
    );

    return true;
}

NTSTATUS NtWaitForSingleObject(HANDLE object_handle, bool /*alertable*/,
                               const LARGE_INTEGER* timeout) {
    auto objrw = deref_handle(object_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    // FIXME - check handle has SYNCHRONIZE access

    optional<uint64_t> t;

    if (timeout) {
        if (timeout->QuadPart > 0) {
            print("FIXME - NtWaitForSingleObject positive timeout\n");
            return nt_status::NOT_SUPPORTED;
        }

        t = -timeout->QuadPart;
    }

    // FIXME - non-alertable waits

    return wait_for_object(objrw.get(), true, t);
}

static nt_status wait_for_objects(span<object*> objects, bool wait_any,
                                  optional<uint64_t> timeout) {
#ifdef DEBUG
    uint64_t cr8 = 0, rflags = 0;

    __asm __volatile(
        "mov rax, cr8\n"
        "mov %0, rax\n"
        :
        : "m" (cr8)
        : "rax"
    );

    if (cr8)
        bsod("calling wait_for_objects when cr8 != 0");

    __asm __volatile(
        "pushf\n"
        "pop rax\n"
        "mov %0, rax\n"
        :
        : "m" (rflags)
        : "rax"
    );

    if (!(rflags & 0x200)) // IF flag not set
        bsod("calling wait_for_objects with interrupts disabled");
#endif
    auto current_thread = get_current_thread();

    __asm("cli");
    acquire_sync_lock();

    if (current_thread->status == thread_status::dead)
        yield(); // release sync_lock, doesn't return

    if (wait_any) {
        for (unsigned int i = 0; i < objects.size(); i++) {
            auto& obj = *objects[i];
            auto& so = object_get_sync_object(obj);

            if (so.signalled) {
                object_on_wake(obj);
                release_sync_lock();
                __asm("sti");

                return (enum nt_status)((unsigned int)nt_status::SUCCESS + i);
            }
        }
    } else {
        unsigned int signalled_count = 0;

        for (auto* obj : objects) {
            auto& so = object_get_sync_object(*obj);

            if (so.signalled)
                signalled_count++;
        }

        if (signalled_count == objects.size()) {
            for (auto* obj : objects) {
                object_on_wake(*obj);
            }

            release_sync_lock();
            __asm("sti");

            return nt_status::SUCCESS;
        }
    }

    if (timeout.has_value() && *timeout == 0) {
        release_sync_lock();
        __asm("sti");
        return nt_status::TIMEOUT;
    }

    // do actual waiting

    size_t mwb_size = offsetof(multiple_wait_block, entries) + (objects.size() * sizeof(waiter_object_multiple));
    auto mwb = (multiple_wait_block*)malloc(mwb_size);
    if (!mwb) {
        release_sync_lock();
        __asm("sti");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    mwb->t = current_thread;
    mwb->left = wait_any ? 1 : (unsigned int)objects.size();
    mwb->num_entries = (unsigned int)objects.size();

    for (size_t i = 0; i < objects.size(); i++) {
        auto ent = new (&mwb->entries[i]) waiter_object_multiple;

        auto& so = object_get_sync_object(*objects[i]);

        insert_tail_list(so.waiters, ent->wait_le);

        ent->block = mwb;
    }

    current_thread->multwait = mwb;

    auto& cb = *get_cpu_block();

    current_thread->status = thread_status::waiting_multiple;
    current_thread->current_cpu = 0xffffffff;
    current_thread->wait_interruptible = true;

    if (current_thread->proc)
        current_thread->proc->running_cpus &= ~((uint64_t)1 << cb.cpu_no);

    if (!timeout.has_value())
        current_thread->wait_timeout = 0;
    else {
        current_thread->wait_timeout = *timeout / (SYSTEM_TIME_RESOLUTION / TICK_RATE);

        if (*timeout % (SYSTEM_TIME_RESOLUTION / TICK_RATE))
            current_thread->wait_timeout++;

        insert_tail_list(thread_list_timeout, current_thread->timeout_le);
    }

    yield(); // releases sync_lock

    auto index = mwb->index;
    free(mwb);

    // FIXME - non-alertable waits
    if (current_thread->pending_signal)
        return nt_status::ALERTED;

    if (timeout.has_value() && current_thread->wait_timeout == 0)
        return nt_status::TIMEOUT;

    if (!wait_any)
        return nt_status::SUCCESS;

    return (enum nt_status)((unsigned int)nt_status::SUCCESS + index);
}

NTSTATUS NtWaitForMultipleObjects(uint32_t object_count, HANDLE* objects_array,
                                  bool wait_any, bool alertable,
                                  const LARGE_INTEGER* timeout) {
    if (object_count == 1)
        return NtWaitForSingleObject(objects_array[0], alertable, timeout);

    optional<uint64_t> t;

    if (timeout) {
        if (timeout->QuadPart > 0) {
            print("FIXME - NtWaitForMultipleObjects positive timeout\n");
            return nt_status::NOT_SUPPORTED;
        }

        t = -timeout->QuadPart;
    }

    array<object*, MAXIMUM_WAIT_OBJECTS> objs;

    for (unsigned int i = 0; i < object_count; i++) {
        // FIXME - check handles have SYNCHRONIZE access

        auto objrw = deref_handle(objects_array[i]);
        if (!objrw) {
            for (unsigned int j = 0; j < i; j++) {
                object_release(*objs[j]);
            }
            return nt_status::INVALID_HANDLE;
        }

        if (!object_is_sync_object(objrw.get())) {
            for (unsigned int j = 0; j < i; j++) {
                object_release(*objs[j]);
            }
            return nt_status::INVALID_PARAMETER;
        }

        // if !wait_any, make sure that no duplicates (because of object_on_wake)
        if (!wait_any) {
            for (unsigned int j = 0; j < i; j++) {
                if (objs[j] == &objrw.get())
                    return nt_status::INVALID_PARAMETER;
            }
        }

        object_acquire(objrw.get());
        objs[i] = &objrw.get();
    }

    auto ret = wait_for_objects(span(objs.data(), object_count), wait_any, t);

    for (unsigned int i = 0; i < object_count; i++) {
        object_release(*objs[i]);
    }

    return ret;
}

void disable_preempt() {
    __asm __volatile(
        "inc QWORD PTR gs:%0\n"
        :
        : "i" (offsetof(cpu_block, disable_preempt))
    );
}

void enable_preempt() {
    __asm __volatile(
        "dec QWORD PTR gs:%0\n"
        :
        : "i" (offsetof(cpu_block, disable_preempt))
    );
}

void set_current_thread_waiting() {
    // must have synclock held

    auto& cb = *get_cpu_block();
    auto t = cb.current_thread;

    t->status = thread_status::waiting;
    t->current_cpu = 0xffffffff;
    t->wait_interruptible = false;
    t->wait_timeout = 0;

    if (t->proc)
        t->proc->running_cpus &= ~((uint64_t)1 << cb.cpu_no);

    yield();
}
