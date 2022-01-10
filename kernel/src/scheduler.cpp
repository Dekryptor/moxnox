#include "kernel.h"
#include "objects.h"
#include "cpu_block.h"
#include "unique_handle.h"
#include "spinlock.h"
#include <cstdlib>
#include <optional>
#include <atomic>

using namespace std;

uint16_t last_tid = 0; // FIXME - too small?

static const uint16_t kernel_CS = 0x8;
static const uint16_t kernel_DS = 0x10;
static const uint64_t time_slice = 50; // ticks

list_entry thread_list = { &thread_list, &thread_list };

static atomic<uint64_t> cpu_block_va = 0xfffffa3000000000;

volatile uint64_t sync_lock_val = 0;

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

static void kernel_thread_start(void (*func)(void*, void*), void* param1, void* param2) {
    func(param1, param2);

    get_current_thread()->terminate();
}

unique_handle add_kernel_thread(void (*func)(void*, void*), void* param1, void* param2) {
    uint16_t tid;

    auto tptr = new thread;
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

    auto stack = (void*)allocate_va(KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES);
    if (!stack) {
        print("allocate_va failed\n");
        delete &t;
        return {};
    }

    for (unsigned int i = 0; i < KERNEL_STACK_PAGES; i++) {
        auto pa = allocate_page();
        if (!pa) {
            print("allocate_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                auto va = (uintptr_t)stack + ((j + KERNEL_GUARD_PAGES) << PAGE_SHIFT);
                auto pa = va_to_pa(va);

                free_page(pa);
                unmap_page(va);
            }

            free_va((uintptr_t)stack, KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES);

            delete &t;
            return {};
        }

        if (!map_page(pa, (uintptr_t)stack + ((i + KERNEL_GUARD_PAGES) << PAGE_SHIFT), mapping_type::read_write)) {
            print("map_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                auto va = (uintptr_t)stack + ((j + KERNEL_GUARD_PAGES) << PAGE_SHIFT);
                auto pa = va_to_pa(va);

                free_page(pa);
                unmap_page(va);
            }

            free_va((uintptr_t)stack, KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES);

            delete &t;
            return {};
        }
    }

    t.stack = stack;

    memset(&t.context, 0, sizeof(t.context));
    t.context.rip = (uintptr_t)kernel_thread_start;
    t.context.rcx = (uintptr_t)func;
    t.context.rdx = (uintptr_t)param1;
    t.context.r8 = (uintptr_t)param2;
    t.context.rsp = (uintptr_t)((uint8_t*)t.stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT) - 0x20); // 0x20 because of MS ABI
    t.context.rflags = 0x202; // IF, reserved flag
    t.context.cs = kernel_CS;
    t.context.ds = kernel_DS;
    t.context.es = kernel_DS;
    t.context.fs = kernel_DS;
    t.context.gs = kernel_DS;
    t.context.ss = kernel_DS;
    t.kernel_mode = true;

    t.status = thread_status::runnable;

    {
        sync_lock sl;
        lock_guard lock(sl);
        insert_tail_list(thread_list, t.le);
    }

    auto hv = add_handle(t, 0); // FIXME - access mask

    // we don't decrease the refcount after the handle is created, because of the active thread list

    return unique_handle{hv};
}

unique_handle add_kernel_thread(void (*func)(void*), void* param) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
    return add_kernel_thread((void(*)(void*, void*))func, param, nullptr);
#pragma GCC diagnostic pop
}

unique_handle add_kernel_thread(void (*func)()) {
    return add_kernel_thread((void(*)(void*, void*))func, nullptr, nullptr);
}

static thread* pick_runnable_thread(thread* current) {
    list_entry* le;
    auto cb = get_cpu_block();
    uint64_t mask = 1 << cb->cpu_no; // FIXME - is the local APIC ID guaranteed to start at 0?

    // FIXME - proper scheduling algorithm (currently just loops through runnable threads)

    auto consider = [&](thread& t) -> thread* {
        if (t.status == thread_status::runnable && t.affinity & mask)
            return &t;

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

extern "C"
void scheduler() {
    auto cb = get_cpu_block();

    if (cb->current_thread) {
        bool dead = false;

        acquire_sync_lock();

        memcpy(&cb->current_thread->context, &cb->thread_context, sizeof(amd64_context));

        if (cb->current_thread->status == thread_status::dead) {
            remove_entry_list(cb->current_thread->le);
            dead = true;
        } else if (cb->current_thread->status == thread_status::running)
            cb->current_thread->status = thread_status::runnable;
        else
            cb->current_thread = nullptr;

        release_sync_lock();

        if (dead) {
            cb->current_thread->release();
            cb->current_thread = nullptr;
        }
    }

    do {
        acquire_sync_lock();

        cb->current_thread = pick_runnable_thread(cb->current_thread);

        if (cb->current_thread)
            cb->current_thread->status = thread_status::running;

        release_sync_lock();

        if (cb->current_thread) {
            memcpy(&cb->thread_context, &cb->current_thread->context, sizeof(amd64_context));
            cb->time_slice_left = time_slice;
            __asm("jmp scheduler_restore"); // iretq will re-enable interrupts
        }

        __asm("sti; hlt; cli");
    } while (true);
}

span<char> sp_context(span<char>dest, const amd64_context& ctx) {
    dest = sp_strcpy(dest, "rax=");
    dest = sp_itoa_hex(dest, ctx.rax, 16);
    dest = sp_strcpy(dest, " rcx=");
    dest = sp_itoa_hex(dest, ctx.rcx, 16);
    dest = sp_strcpy(dest, " rdx=");
    dest = sp_itoa_hex(dest, ctx.rdx, 16);
    dest = sp_strcpy(dest, "\n");

    dest = sp_strcpy(dest, "rbx=");
    dest = sp_itoa_hex(dest, ctx.rbx, 16);
    dest = sp_strcpy(dest, " rsp=");
    dest = sp_itoa_hex(dest, ctx.rsp, 16);
    dest = sp_strcpy(dest, " rsi=");
    dest = sp_itoa_hex(dest, ctx.rsi, 16);
    dest = sp_strcpy(dest, "\n");

    dest = sp_strcpy(dest, "rbx=");
    dest = sp_itoa_hex(dest, ctx.rbx, 16);
    dest = sp_strcpy(dest, " rsp=");
    dest = sp_itoa_hex(dest, ctx.rsp, 16);
    dest = sp_strcpy(dest, " rsi=");
    dest = sp_itoa_hex(dest, ctx.rsi, 16);
    dest = sp_strcpy(dest, "\n");

    dest = sp_strcpy(dest, "rdi=");
    dest = sp_itoa_hex(dest, ctx.rdi, 16);
    dest = sp_strcpy(dest, "  r8=");
    dest = sp_itoa_hex(dest, ctx.r8, 16);
    dest = sp_strcpy(dest, "  r9=");
    dest = sp_itoa_hex(dest, ctx.r9, 16);
    dest = sp_strcpy(dest, "\n");

    dest = sp_strcpy(dest, "r10=");
    dest = sp_itoa_hex(dest, ctx.r10, 16);
    dest = sp_strcpy(dest, " r11=");
    dest = sp_itoa_hex(dest, ctx.r11, 16);
    dest = sp_strcpy(dest, " r12=");
    dest = sp_itoa_hex(dest, ctx.r12, 16);
    dest = sp_strcpy(dest, "\n");

    dest = sp_strcpy(dest, "r13=");
    dest = sp_itoa_hex(dest, ctx.r13, 16);
    dest = sp_strcpy(dest, " r14=");
    dest = sp_itoa_hex(dest, ctx.r14, 16);
    dest = sp_strcpy(dest, " r15=");
    dest = sp_itoa_hex(dest, ctx.r15, 16);
    dest = sp_strcpy(dest, "\n");

    dest = sp_strcpy(dest, "rip=");
    dest = sp_itoa_hex(dest, ctx.rip, 16);
    dest = sp_strcpy(dest, " rflags=");
    dest = sp_itoa_hex(dest, ctx.rflags, 8);
    dest = sp_strcpy(dest, "\n");

    dest = sp_strcpy(dest, "cs=");
    dest = sp_itoa_hex(dest, ctx.cs, 4);
    dest = sp_strcpy(dest, " ds=");
    dest = sp_itoa_hex(dest, ctx.ds, 4);
    dest = sp_strcpy(dest, " es=");
    dest = sp_itoa_hex(dest, ctx.es, 4);
    dest = sp_strcpy(dest, " fs=");
    dest = sp_itoa_hex(dest, ctx.fs, 4);
    dest = sp_strcpy(dest, " gs=");
    dest = sp_itoa_hex(dest, ctx.gs, 4);
    dest = sp_strcpy(dest, " ss=");
    dest = sp_itoa_hex(dest, ctx.ss, 4);
    dest = sp_strcpy(dest, "\n");

    return dest;
}

thread::~thread() {
    if (stack) {
        auto va = (uintptr_t)stack + (KERNEL_GUARD_PAGES << PAGE_SHIFT);
        for (unsigned int i = 0; i < KERNEL_STACK_PAGES; i++) {
            auto pa = va_to_pa(va);

            free_page(pa);
            unmap_page(va);

            va += PAGE_SIZE;
        }

        free_va((uintptr_t)stack, KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES);
    }

    if (proc)
        proc->release();
}

static void wait_for_object(object& obj) {
    auto current_thread = get_current_thread();
    optional<bool> sigopt;

    __asm("cli");
    acquire_sync_lock();

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_sync_object<remove_reference_t<decltype(o)>>)
            sigopt = o.signalled;
        else
            o.release();
    });

    if (!sigopt.has_value()) {
        release_sync_lock();
        __asm("sti");
        // FIXME - exception
        print("Not a sync object\n");
        return;
    }

    auto signalled = *sigopt;

    if (signalled) {
        visit_obj(obj, [&](auto&& o) {
            if constexpr (object_has_on_wake<remove_reference_t<decltype(o)>>)
                o.on_wake();
        });

        release_sync_lock();
        __asm("sti");

        visit_obj(obj, [&](auto&& o) {
            o.release();
        });

        return;
    }

    current_thread->waiting_for = obj;
    current_thread->status = thread_status::waiting;

    yield(); // releases sync_lock
}

void wait_for_object(handle_t h) {
    auto objrw = deref_handle(h);
    if (!objrw) {
        // FIXME - exception
        print("No such handle\n");
        return;
    }

    // FIXME - check handle has SYNCHRONIZE access

    wait_for_object(objrw->get());
}

bool init_cpu_block(bool handle_timers, unsigned int cpu_no) {
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

    cb.self = &cb;
    cb.current_thread = nullptr;
    cb.handle_timers = handle_timers;
    cb.cpu_no = cpu_no;
    cb.kernel_mode = true;

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
        : "m" (&cb)
        : "ecx", "rax", "rdx"
    );

    return true;
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
