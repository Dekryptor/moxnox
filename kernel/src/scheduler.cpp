#include "kernel.h"
#include "thread.h"
#include <cstdlib>

static uint16_t last_tid = 0; // FIXME - too small?

static const unsigned int KERNEL_STACK_PAGES = 4; // 16KB
static const uint16_t kernel_CS = 0x8;
static const uint16_t kernel_SS = 0x10;
static const uint64_t time_slice = 50; // ticks

static const uint64_t thread_stack_block = 0xfffffa1000000000;

list_entry thread_list = { &thread_list, &thread_list };

// FIXME - SMP
amd64_context thread_context;
thread* current_thread = nullptr;
uint64_t time_slice_left;

void terminate_thread() {
    current_thread->status = thread_status::dead;
    yield();
}

static void kernel_thread_start(void (*func)(void*), void* param) {
    func(param);

    terminate_thread();
}

bool add_kernel_thread(void (*func)(void*), void* param) {
    uint16_t tid;

    // FIXME - spinlock for last_tid (or use atomics)
    if (last_tid == 0xffff) { // FIXME - check for lowest unused number
        print("Thread limit reached.\n");
        return false;
    }

    last_tid++;
    tid = last_tid;

    auto tptr = new thread;
    if (!tptr) {
        print("new failed\n");
        return false;
    }

    auto& t = *tptr;

    t.tid = tid;

    auto stack = (void*)(thread_stack_block | (tid << 20));

    for (unsigned int i = 0; i < KERNEL_STACK_PAGES; i++) {
        auto pa = allocate_page();
        if (!pa) {
            print("allocate_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                unmap_and_free_page((uintptr_t)stack + (j << PAGE_SHIFT));
            }

            delete &t;
            return false;
        }

        if (!map_page(pa, (uintptr_t)stack + (i << PAGE_SHIFT))) {
            print("map_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                unmap_and_free_page((uintptr_t)stack + (j << PAGE_SHIFT));
            }

            delete &t;
            return false;
        }
    }

    t.stack = stack;

    t.context.rip = (uintptr_t)kernel_thread_start;
    t.context.rcx = (uintptr_t)func;
    t.context.rdx = (uintptr_t)param;
    t.context.rsp = (uintptr_t)((uint8_t*)t.stack + (KERNEL_STACK_PAGES << PAGE_SHIFT) - 0x20); // 0x20 because of MS ABI
    t.context.rflags = 0x202; // IF, reserved flag
    t.context.cs = kernel_CS;
    t.context.ss = kernel_SS;

    t.status = thread_status::runnable;

    // FIXME - need spinlock
    insert_tail_list(thread_list, t.le);

    return true;
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

        if (&t == current_thread)
            break;

        le = le->next;
    }

    return nullptr;
}

extern "C"
void scheduler() {
    // FIXME - for SMP, get thread_list spinlock (interrupts are disabled already)

    if (current_thread) {
        if (current_thread->status == thread_status::running)
            current_thread->status = thread_status::runnable;

        memcpy(&current_thread->context, &thread_context, sizeof(thread_context));

        if (current_thread->status == thread_status::dead) {
            remove_entry_list(current_thread->le);
            delete current_thread;

            current_thread = nullptr;
        }
    }

    do {
        current_thread = pick_runnable_thread(current_thread);

        if (current_thread) {
            memcpy(&thread_context, &current_thread->context, sizeof(thread_context));
            current_thread->status = thread_status::running;
            time_slice_left = time_slice;
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
    print(" cs=");
    print_hex(ctx.cs, 4);
    print(" ss=");
    print_hex(ctx.ss, 4);
    print("\n");
}

thread::~thread() {
    if (stack) {
        for (unsigned int i = 0; i < KERNEL_STACK_PAGES; i++) {
            unmap_and_free_page((uintptr_t)stack | (i << PAGE_SHIFT));
        }
    }
}
