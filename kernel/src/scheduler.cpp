#include "kernel.h"
#include "objects.h"
#include "cpu_block.h"
#include "unique_handle.h"
#include "spinlock.h"
#include <cstdlib>
#include <optional>
#include <atomic>

using namespace std;

static uint16_t last_tid = 0; // FIXME - too small?

static const uint16_t kernel_CS = 0x8;
static const uint16_t kernel_DS = 0x10;
static const uint64_t time_slice = 50; // ticks

list_entry thread_list = { &thread_list, &thread_list };

static atomic<uint64_t> cpu_block_va = 0xfffffa3000000000;

volatile uint64_t sync_lock_val = 0;

thread* get_current_thread() {
    return get_cpu_block()->current_thread;
}

void terminate_thread() {
    auto current_thread = get_current_thread();
    unsigned int release_count = 0;

    current_thread->status = thread_status::dead;
    current_thread->signalled = true;

    {
        auto obj = static_cast<object*>(current_thread);

        lock_guard lock(sync_lock);

        auto le = thread_list.next;

        while (le != &thread_list) {
            auto& t = containing_record<thread>(le);

            if (t.status == thread_status::waiting && t.waiting_for.has_value() && &t.waiting_for->get() == obj) {
                t.status = thread_status::runnable;
                t.waiting_for = nullopt;
                release_count++;
            }

            le = le->next;
        }
    }

    // out of spinlock
    for (unsigned int i = 0; i < release_count; i++) {
        current_thread->release();
    }

    yield();
}

static void kernel_thread_start(void (*func)(void*), void* param) {
    func(param);

    terminate_thread();
}

unique_handle add_kernel_thread(void (*func)(void*), void* param) {
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
    t.context.rdx = (uintptr_t)param;
    t.context.rsp = (uintptr_t)((uint8_t*)t.stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT) - 0x20); // 0x20 because of MS ABI
    t.context.rflags = 0x202; // IF, reserved flag
    t.context.cs = kernel_CS;
    t.context.ds = kernel_DS;
    t.context.es = kernel_DS;
    t.context.fs = kernel_DS;
    t.context.gs = kernel_DS;
    t.context.ss = kernel_DS;

    t.status = thread_status::runnable;

    {
        lock_guard lock(sync_lock);
        insert_tail_list(thread_list, t.le);
    }

    auto hv = add_handle(t, 0); // FIXME - access mask

    // we don't decrease the refcount after the handle is created, because of the active thread list

    return unique_handle{hv};
}

static thread* pick_runnable_thread(thread* current) {
    list_entry* le;

    // FIXME - proper scheduling algorithm (currently just loops through runnable threads)

    auto consider = [](thread& t) -> thread* {
        if (t.status == thread_status::runnable)
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
        memcpy(&cb->current_thread->context, &cb->thread_context, sizeof(amd64_context));

        if (cb->current_thread->status == thread_status::dead) {
            acquire_sync_lock();
            remove_entry_list(cb->current_thread->le);
            release_sync_lock();

            cb->current_thread->release();

            cb->current_thread = nullptr;
        }
    }

    do {
        acquire_sync_lock();

        if (cb->current_thread && cb->current_thread->status == thread_status::running)
            cb->current_thread->status = thread_status::runnable;

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

void print_context(const amd64_context& ctx) {
    print("rax=");
    print_hex(ctx.rax, 16);
    print(" rcx=");
    print_hex(ctx.rcx, 16);
    print(" rdx=");
    print_hex(ctx.rdx, 16);
    print("\n");

    print("rbx=");
    print_hex(ctx.rbx, 16);
    print(" rsp=");
    print_hex(ctx.rsp, 16);
    print(" rsi=");
    print_hex(ctx.rsi, 16);
    print("\n");

    print("rbx=");
    print_hex(ctx.rbx, 16);
    print(" rsp=");
    print_hex(ctx.rsp, 16);
    print(" rsi=");
    print_hex(ctx.rsi, 16);
    print("\n");

    print("rdi=");
    print_hex(ctx.rdi, 16);
    print("  r8=");
    print_hex(ctx.r8, 16);
    print("  r9=");
    print_hex(ctx.r9, 16);
    print("\n");

    print("r10=");
    print_hex(ctx.r10, 16);
    print(" r11=");
    print_hex(ctx.r11, 16);
    print(" r12=");
    print_hex(ctx.r12, 16);
    print("\n");

    print("r13=");
    print_hex(ctx.r13, 16);
    print(" r14=");
    print_hex(ctx.r14, 16);
    print(" r15=");
    print_hex(ctx.r15, 16);
    print("\n");

    print("rip=");
    print_hex(ctx.rip, 16);
    print(" rflags=");
    print_hex(ctx.rflags, 8);
    print("\n");

    print("cs=");
    print_hex(ctx.cs, 4);
    print(" ds=");
    print_hex(ctx.ds, 4);
    print(" es=");
    print_hex(ctx.es, 4);
    print(" fs=");
    print_hex(ctx.fs, 4);
    print(" gs=");
    print_hex(ctx.gs, 4);
    print(" ss=");
    print_hex(ctx.ss, 4);
    print("\n");
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
}

void wait_for_object(object& obj) {
    auto current_thread = get_current_thread();
    optional<bool> sigopt;

    {
        optional<lock_guard<decltype(sync_lock)>> lock(sync_lock);

        visit_obj(obj, [&](auto&& o) {
            if constexpr (is_sync_object<remove_reference_t<decltype(o)>>)
                sigopt = o.signalled;
            else
                o.release();
        });

        if (!sigopt.has_value()) {
            lock.reset();
            // FIXME - exception
            print("Not a sync object\n");
            return;
        }

        auto signalled = *sigopt;

        if (signalled) {
            visit_obj(obj, [&](auto&& o) {
                if constexpr (is_same_v<remove_reference_t<decltype(o)>, timer>) {
                    if (o.periodic)
                        o.signalled = false;
                } else if constexpr (is_same_v<remove_reference_t<decltype(o)>, event>) {
                    if (o.autoreset)
                        o.signalled = false;
                }
            });

            lock.reset();

            visit_obj(obj, [&](auto&& o) {
                o.release();
            });

            return;
        }

        current_thread->waiting_for = obj;
        current_thread->status = thread_status::waiting;
    }

    yield();
}

void wait_for_object(handle_t h) {
    __asm("cli");

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
    cb.sync_lock_cr8 = 0;

    auto stack = allocate_stack();
    if (!stack)
        return false;

    cb.scheduler_stack = ((uint8_t*)stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT)) - 0x20; // 0x20 for MS ABI

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
