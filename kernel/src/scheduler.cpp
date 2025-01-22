#include "kernel.h"
#include "objects.h"
#include "cpu_block.h"
#include "spinlock.h"
#include "handle.h"
#include <cstdlib>
#include <optional>
#include <atomic>

using namespace std;

uint16_t last_tid = 0; // FIXME - too small?

static const uint16_t kernel_CS = 0x8;
static const uint16_t kernel_DS = 0x10;
static const uint64_t time_slice = 50; // ticks

list_entry thread_list = { &thread_list, &thread_list };
list_entry thread_list_dead = { &thread_list_dead, &thread_list_dead };

static atomic<uint64_t> cpu_block_va = 0xfffffa3000000000;

volatile uint64_t sync_lock_val = 0;

#ifdef DEBUG
uint32_t sync_lock_cpu = 0xffffffff;
#endif

static void kernel_thread_start(void (*func)(void*, void*), void* param1, void* param2) {
    func(param1, param2);

    get_current_thread()->terminate();
}

object_ref add_kernel_thread(void (*func)(void*, void*), void* param1, void* param2) {
    uint16_t tid;

    auto tptr = new (align_val_t(16)) thread;
    if (!tptr) {
        printk("new failed\n");
        return {};
    }

    auto& t = *tptr;

    // FIXME - spinlock for last_tid (or use atomics)
    if (last_tid == 0xffff) { // FIXME - check for lowest unused number
        printk("Thread limit reached.\n");
        delete &t;
        return {};
    }

    last_tid++;
    tid = last_tid;

    t.tid = tid;

    t.kernel_stack = allocate_stack();
    if (!t.kernel_stack) {
        printk("allocate_stack failed\n");
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

static thread* pick_runnable_thread(thread* current) {
    list_entry* le;
    auto cb = get_cpu_block();
    uint64_t mask = 1 << cb->cpu_no; // FIXME - is the local APIC ID guaranteed to start at 0?

    // FIXME - proper scheduling algorithm (currently just loops through runnable threads)

    auto consider = [&](thread& t) -> thread* {
        if (t.status == thread_status::runnable && t.affinity & mask && t.suspend_count == 0)
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

span<char> sp_context(span<char>dest, const AMD64_CONTEXT& ctx) {
    dest = sp_strcpy(dest, "rax=");
    dest = sp_itoa_hex(dest, ctx.Rax, 16);
    dest = sp_strcpy(dest, " rcx=");
    dest = sp_itoa_hex(dest, ctx.Rcx, 16);
    dest = sp_strcpy(dest, " rdx=");
    dest = sp_itoa_hex(dest, ctx.Rdx, 16);
    dest = sp_strcpy(dest, "\n");

    dest = sp_strcpy(dest, "rbx=");
    dest = sp_itoa_hex(dest, ctx.Rbx, 16);
    dest = sp_strcpy(dest, " rsp=");
    dest = sp_itoa_hex(dest, ctx.Rsp, 16);
    dest = sp_strcpy(dest, " rbp=");
    dest = sp_itoa_hex(dest, ctx.Rbp, 16);
    dest = sp_strcpy(dest, "\n");

    dest = sp_strcpy(dest, "rsi=");
    dest = sp_itoa_hex(dest, ctx.Rsi, 16);
    dest = sp_strcpy(dest, " rdi=");
    dest = sp_itoa_hex(dest, ctx.Rdi, 16);
    dest = sp_strcpy(dest, "  r8=");
    dest = sp_itoa_hex(dest, ctx.R8, 16);
    dest = sp_strcpy(dest, "\n");

    dest = sp_strcpy(dest, " r9=");
    dest = sp_itoa_hex(dest, ctx.R9, 16);
    dest = sp_strcpy(dest, " r10=");
    dest = sp_itoa_hex(dest, ctx.R10, 16);
    dest = sp_strcpy(dest, " r11=");
    dest = sp_itoa_hex(dest, ctx.R11, 16);
    dest = sp_strcpy(dest, "\n");

    dest = sp_strcpy(dest, "r12=");
    dest = sp_itoa_hex(dest, ctx.R12, 16);
    dest = sp_strcpy(dest, " r13=");
    dest = sp_itoa_hex(dest, ctx.R13, 16);
    dest = sp_strcpy(dest, " r14=");
    dest = sp_itoa_hex(dest, ctx.R14, 16);
    dest = sp_strcpy(dest, "\n");

    dest = sp_strcpy(dest, "r15=");
    dest = sp_itoa_hex(dest, ctx.R15, 16);
    dest = sp_strcpy(dest, " rip=");
    dest = sp_itoa_hex(dest, ctx.Rip, 16);
    dest = sp_strcpy(dest, " eflags=");
    dest = sp_itoa_hex(dest, ctx.EFlags, 8);
    dest = sp_strcpy(dest, "\n");

    dest = sp_strcpy(dest, "cs=");
    dest = sp_itoa_hex(dest, ctx.SegCs, 4);
    dest = sp_strcpy(dest, " ds=");
    dest = sp_itoa_hex(dest, ctx.SegDs, 4);
    dest = sp_strcpy(dest, " es=");
    dest = sp_itoa_hex(dest, ctx.SegEs, 4);
    dest = sp_strcpy(dest, " fs=");
    dest = sp_itoa_hex(dest, ctx.SegFs, 4);
    dest = sp_strcpy(dest, " gs=");
    dest = sp_itoa_hex(dest, ctx.SegGs, 4);
    dest = sp_strcpy(dest, " ss=");
    dest = sp_itoa_hex(dest, ctx.SegSs, 4);
    dest = sp_strcpy(dest, "\n");

    return dest;
}

nt_status wait_for_object(object& obj, bool interruptible) {
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
    visit_obj(obj, [](auto&& o) {
        o.acquire();
    });

    auto current_thread = get_current_thread();

    __asm("cli");
    acquire_sync_lock();

    if (current_thread->status != thread_status::dead) {
        optional<bool> sigopt;

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
            printk("Not a sync object\n");
            return nt_status::INVALID_PARAMETER;
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

            return nt_status::SUCCESS;
        }

        current_thread->waiting_for = obj;
        current_thread->status = thread_status::waiting;
        current_thread->current_cpu = 0xffffffff;
    }

    yield(); // releases sync_lock

    return nt_status::SUCCESS;
}

bool init_cpu_block(unsigned int cpu_no) {
    auto pages = bytes_to_pages(sizeof(cpu_block));
    auto va = cpu_block_va.fetch_add(pages << PAGE_SHIFT);
    auto& cb = *(cpu_block*)va;

    for (unsigned int i = 0; i < pages; i++) {
        auto pa = allocate_page();
        if (!pa) {
            printk("allocate_page failed\n");
            return false;
        }

        if (!map_page(pa, va, mapping_type::read_write)) {
            printk("map_page failed\n");
            return false;
        }

        va += PAGE_SIZE;
    }

    new (&cb) cpu_block;

    cb.self = &cb;
    cb.current_thread = nullptr;
    cb.cpu_no = cpu_no;

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

NTSTATUS NtWaitForSingleObject(HANDLE object_handle, bool alertable, const LARGE_INTEGER*) {
    // FIXME - time_out

    auto objrw = deref_handle2(object_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    // FIXME - check handle has SYNCHRONIZE access

    return wait_for_object(objrw.get(), alertable);
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
