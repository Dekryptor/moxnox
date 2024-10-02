#include <loaderblock.h>
#include "kernel.h"
#include "unique_handle.h"
#include "context_offsets.h"
#include "objects.h"
#include <string_view>

using namespace std;

extern uint64_t __CTOR_LIST__;

static void call_global_constructors() {
    auto v = &__CTOR_LIST__;

    if (*v != 0xffffffffffffffff)
        return;

    v++;

    while (*v != 0) {
        auto func = reinterpret_cast<void (*)()>(*v);

        func();

        v++;
    }
}

extern "C"
int atexit(void (*)(void)) {
    // needed for linking when there are global objects with destructors
    return 0;
}

#if 0
static void thread_test() {
    auto ev = create_event(false);

    if (!ev) {
        printk("create_event failed\n");
        return;
    }

    auto h = add_kernel_thread([](void* ctx) {
        auto evv = (handle_t)ctx;

        printk("Hello from second thread\n");

        set_event(evv);
    }, (void*)ev.get());

    if (!h) {
        printk("add_kernel_thread failed\n");
        return;
    }

    printk("h = ");
    print_hex(h.get());
    printk("\n");

    wait_for_object(ev.get());
    printk("second thread finished\n");
}

static void timer_test() {
    auto h = create_timer();
    if (!h) {
        printk("create_timer failed\n");
        return;
    }

    printk("Waiting...\n");

    set_timer(h.get(), 5000, false);

    wait_for_object(h.get());

    printk("Timer fired.\n");
}
#endif

static unique_handle start_init_process() {
    auto initout = open_object("tty");
    if (!initout)
        bsod("Could not open tty object.\n");

    auto initout_objrw = deref_handle(initout.get());
    if (!initout_objrw)
        bsod("No such handle\n");

    auto initin = open_object("keyboard");
    if (!initin)
        bsod("Could not open keyboard object.\n");

    auto initin_objrw = deref_handle(initin.get());
    if (!initin_objrw)
        bsod("No such handle\n");

    auto proc_handle = create_process("/init.exe", "/", initin_objrw, initout_objrw, initout_objrw);
    if (!proc_handle)
        bsod("Could not start init process.\n");

    visit_obj(initout_objrw->get(), [&](auto&& o) {
        o.release();
    });

    visit_obj(initin_objrw->get(), [&](auto&& o) {
        o.release();
    });

    if (!success(NtResumeProcess(proc_handle.get())))
        bsod("NtResumeProcess failed for init process.\n");

    return proc_handle;
}

void main_thread() {
    init_keyboard();

    init_syscalls();

    if (!init_smp()) {
        apic_ids = (uint8_t*)malloc(1);
        if (!apic_ids) {
            printk("malloc failed\n");
            return;
        }

        apic_ids[0] = current_cpu_id();
    }

    init_serial();

    protect_kernel_pages();

    init_pci();
    // FIXME - NVMe
    // FIXME - USB

    if (!mount_root())
        bsod("mount_root failed");

    if (!init_fb_console())
        printk("init_fb_console failed\n");

    load_ntdll();

    auto init = start_init_process();

    wait_for_object(init.get());

    bsod("init process died\n");

//     thread_test(); // FIXME
//     timer_test(); // FIXME
}

extern "C"
void startup(loader_block* lb) {
    loader_block_framebuffer fb;

    // FIXME - should restart if anything fails before we can show error message

//     {
//         volatile int wait = 1;
//         while (wait) { }
//     }

    memcpy(&fb, &lb->framebuffer, sizeof(fb));

    if (!init_memory(*lb))
        goto hang;

    lb = nullptr; // no longer valid

    call_global_constructors();

    if (!init_vablock())
        goto hang;

    if (!init_console(fb))
        goto hang;

    if (!init_cpu_block(true, current_cpu_id()))
        goto hang;

    if (!init_console_pipe())
        goto hang;

    printk = safe_print;

    if (!init_interrupts())
        goto hang;

    free_lower_half_mappings();

    if (!add_kernel_thread(main_thread))
        goto hang;

    // FIXME - C++ exceptions

    init_timer(); // leaves interrupts disabled

    __asm __volatile ( // doesn't return
        "mov rsp, gs:%0\n"
        "jmp scheduler\n"
        :
        : "i" (GS_SCHEDULER_STACK)
    );

hang:
    while (1) { }
}
