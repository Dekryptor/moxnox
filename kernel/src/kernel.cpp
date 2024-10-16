#include <loaderblock.h>
#include <version.h>
#include "kernel.h"
#include "unique_handle.h"
#include "context_offsets.h"
#include "objects.h"
#include <string_view>

using namespace std;

extern uint64_t __CTOR_LIST__;

static char cmdline_arr[255];
string_view cmdline;

static bool has_framebuffer;

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
        bsod("Could not open tty object.");

    auto initout_objrw = deref_handle2(initout.get());
    if (!initout_objrw)
        bsod("No such handle");

    auto initin = open_object("keyboard");
    if (!initin)
        bsod("Could not open keyboard object.");

    auto initin_objrw = deref_handle2(initin.get());
    if (!initin_objrw)
        bsod("No such handle");

    auto proc_handle = create_process("/init.exe", "/", initin_objrw, initout_objrw, initout_objrw);
    if (!proc_handle)
        bsod("Could not start init process.");

    if (!success(NtResumeProcess(proc_handle.get())))
        bsod("NtResumeProcess failed for init process.");

    return proc_handle;
}

static void keywait() {
    bool do_wait = false;

    loop_through_cmdline([&](string_view s) {
        if (s == "keywait") {
            do_wait = true;
            return false;
        }

        return true;
    });

    if (!do_wait)
        return;

    auto keybh = open_object2("keyboard");

    if (!keybh) {
        printk("/dev/keyboard not found\n");
        return;
    }

    auto& keyb = keybh.get();

    if (keyb.type != object_type::pipe) {
        printk("/dev/keyboard did not point to a pipe\n");
        return;
    }

    auto& p = static_cast<pipe&>(keyb);
    uint8_t c;

    printk("Press any key to continue...\n");

    // FIXME - make sure this behaves properly if user presses a multi-character key (clear buffer?)
    while (p.read(span(&c, 1)) == 0) { }
}

void main_thread() {
    init_keyboard();

    keywait();

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

    init_rtc();

    if (!mount_root())
        bsod("mount_root failed");

    if (has_framebuffer && !init_fb_console())
        printk("init_fb_console failed\n");

    load_ntdll();

    auto init = start_init_process();

    wait_for_object(init.get());

    auto initrw = deref_handle(init.get());
    auto& init_process = *static_cast<process*>(&initrw->get());

    {
        char s[255];
        auto sp = span(s, sizeof(s));

        sp = sp_strcpy(sp, "init process died (exit code ");
        sp = sp_itoa_hex(sp, (uint64_t)init_process.exit_status);
        sp = sp_strcpy(sp, ")");
        bsod(string_view(s, sp.data() - s));
    }

//     thread_test(); // FIXME
//     timer_test(); // FIXME
}

extern "C"
void startup(loader_block* lb) {
    loader_block_framebuffer fb;

    // FIXME - should restart if anything fails before we can show error message

    memcpy(&fb, &lb->framebuffer, sizeof(fb));

    if (lb->cmdline) {
        size_t len = strlen((char*)lb->cmdline);

        if (len >= sizeof(cmdline_arr))
            len = sizeof(cmdline_arr) - 1;

        memcpy(cmdline_arr, (char*)lb->cmdline, len);
        cmdline_arr[len] = 0;
        cmdline = string_view(cmdline_arr, len);
    }

    has_framebuffer = !!fb.pa;

    if (!init_memory(*lb))
        goto hang;

    lb = nullptr; // no longer valid

    call_global_constructors();

    if (!init_vablock())
        goto hang;

    if (!init_console(fb))
        goto hang;

    printk(MOXNOX_VERSION_STRING "\n");

    if (!init_cpu_block(current_cpu_id()))
        goto hang;

    if (!init_console_pipe())
        goto hang;

    _printk = safe_print;

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
