#include <loaderblock.h>
#include <version.h>
#include <span>
#include <atomic>
#include <string.h>
#include "context_offsets.h"
import objects;
#include "process.h"
#include "scheduler.h"
#include <string_view>
#include "thread.h"

import print;
import bsod;
import acpi;
import null;
import mem;
import list;
import interrupts;
import serial;
import pci;
import vfs;
import pe;
import chardev;
import timer;
import syscalls;
import smp;
import console;
import evdev;
import keyboard;
import unicode;
import section;
import process;
import image;
import cmdline;
import thread;

using namespace std;

extern uint64_t __CTOR_LIST__;

static char cmdline_arr[255];
static loader_block_framebuffer lb_fb;
static loader_block_text_console lb_tc;

void (*_printk)(string_view s) = early_print;

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

extern "C"
void __cxa_pure_virtual() {
    // Clang wants this
    bsod("__cxa_pure_virtual() called");
}

static object_ref create_init_process() {
    auto& p = static_cast<chardev&>(console_chardev.get());

    p.acquire();

    object_ref initin;
    object_ref initout{p};

    if (console_type == console_type::other) {
        p.acquire();
        initin = object_ref{p};
    } else {
        auto status = open_object("null", initin);
        if (!success(status))
            bsod_format("Error {} opening /dev/null", status);
    }

    static const u16string_view init_cmdline_pref = u"init.exe ";

    auto init_cmdline_len = init_cmdline_pref.size() + utf8_to_utf16_len(cmdline);

    auto init_cmdline = (char16_t*)malloc(init_cmdline_len * sizeof(char16_t));
    if (!init_cmdline)
        bsod("malloc failed while creating init process");

    memcpy(init_cmdline, init_cmdline_pref.data(),
           init_cmdline_pref.size() * sizeof(char16_t));

    utf8_to_utf16(span(init_cmdline + init_cmdline_pref.size(),
                       init_cmdline_len - init_cmdline_pref.size()), cmdline);

    auto ref = create_process("/init.exe", "/", u16string_view(init_cmdline, init_cmdline_len),
                              initin, initout, initout);

    free(init_cmdline);

    return ref;
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

    chardev* p = nullptr;

    if (console_type == console_type::other) {
        if (!console_chardev)
            return;

        p = static_cast<chardev*>(&console_chardev.get());
        p->acquire();
    } else {
        object_ref keybh;

        auto status = open_object("keyboard", keybh);

        if (!success(status)) {
            print("Error {} while trying to open /dev/keyboard\n", status);
            return;
        }

        auto& keyb = keybh.get();

        if (keyb.type != object_type::chardev) {
            print("/dev/keyboard did not point to a character device\n");
            return;
        }

        p = static_cast<chardev*>(&keyb);
    }

    if (!p)
        return;

    object_ref obj{*p}; // so it gets released automatically

    print("Press any key to continue...\n\e[?25l");

    uint8_t c;
    uint64_t read_bytes;
    nt_status status;

    status = p->read(span(&c, 1), read_bytes);
    if (!success(status))
        print("pipe::read returned {}\n", status);

    p->clear();

    print("\e[?25h");
}

void main_thread() {
    print("\e[2J\e[1;1H");  // erase screen, move to top left

    acpi_early_init_done = true;

    free_lower_half_mappings();

    setup_folios();

    scan_pci();
    init_pci(true);

    if (!init_console(lb_fb, lb_tc))
        bsod("init_console failed\n");

    if (!init_acpi_done) {
        init_acpi();
        find_acpi_devices();
        init_acpi_done = true;
    }

    if (!init_keyboard_pipe())
        print("init_keyboard_pipe failed\n");

    // if no ACPI, look for serial port at its traditional address
    if (rsdp_pa == 0)
        init_serial(0x3f8, 4, "");

    _printk = safe_print;

    if (console_type == console_type::framebuffer && !init_fb_console())
        print("init_fb_console failed\n");

    print(MOXNOX_VERSION_STRING "\n");

    keywait();

    init_pci(false);

    init_syscalls();

    if (!init_smp()) {
        apic_ids = (uint8_t*)malloc(1);
        if (!apic_ids) {
            print("malloc failed\n");
            return;
        }

        apic_ids[0] = current_cpu_id();
    }

    protect_kernel_pages();

    if (!create_dev_null())
        print("create_dev_null failed\n");

    if (!mount_root())
        bsod("mount_root failed");

    load_ntdll();

    auto init = create_init_process();

    if (!init)
        bsod("Could not start init process.");

    static_cast<process&>(init.get()).resume();

    wait_for_object(init.get(), false);

    auto& init_process = static_cast<process&>(init.get());

    bsod_format("init process died (exit code {})", init_process.exit_status);
}

// FIXME - should be in string module, but currently ICEs GCC 15 if placed there
extern "C"
int toupper(int c) {
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';
    else
        return c;
}

extern "C"
void startup(loader_block* lb) {
    span<const uint8_t> debug_info, debug_abbrev, debug_rnglists, debug_str,
                        debug_addr, debug_str_offsets;

    // FIXME - should restart if anything fails before we can show error message

    if (lb->text_mode)
        memcpy(&lb_tc, &lb->text_console, sizeof(lb_tc));
    else
        memcpy(&lb_fb, &lb->framebuffer, sizeof(lb_fb));

    if (lb->cmdline) {
        size_t len = strlen((char*)lb->cmdline);

        if (len >= sizeof(cmdline_arr))
            len = sizeof(cmdline_arr) - 1;

        memcpy(cmdline_arr, (char*)lb->cmdline, len);
        cmdline_arr[len] = 0;
        cmdline = string_view(cmdline_arr, len);
    }

    if (lb->edid)
        copy_edid(lb);

    if (lb->text_mode)
        console_type = console_type::text;
    else
        console_type = console_type::framebuffer;

    debug_info = span((uint8_t*)(uintptr_t)lb->debug_info_ptr, lb->debug_info_size);
    debug_abbrev = span((uint8_t*)(uintptr_t)lb->debug_abbrev_ptr, lb->debug_abbrev_size);
    debug_rnglists = span((uint8_t*)(uintptr_t)lb->debug_rnglists_ptr, lb->debug_rnglists_size);
    debug_str = span((uint8_t*)(uintptr_t)lb->debug_str_ptr, lb->debug_str_size);
    debug_addr = span((uint8_t*)(uintptr_t)lb->debug_addr_ptr, lb->debug_addr_size);
    debug_str_offsets = span((uint8_t*)(uintptr_t)lb->debug_str_offsets_ptr, lb->debug_str_offsets_size);

    rsdp_pa = lb->rsdp_pa;

    if (!init_memory(*lb))
        goto hang;

    lb = nullptr; // no longer valid

    call_global_constructors();

    init_images_list(debug_info, debug_abbrev, debug_rnglists, debug_str,
                     debug_addr, debug_str_offsets);

    if (!init_vablock())
        goto hang;

    if (!init_cpu_block(current_cpu_id()))
        goto hang;

    if (!init_interrupts())
        goto hang;

    init_acpi_early();

    if (!init_apic())
        goto hang;

    __asm("sti");

    if (!add_kernel_thread(main_thread))
        goto hang;

    // FIXME - C++ exceptions

    init_timer(); // leaves interrupts disabled

    acquire_sync_lock();

    __asm __volatile ( // doesn't return
        "mov rsp, gs:%0\n"
        "jmp scheduler\n"
        :
        : "i" (GS_SCHEDULER_STACK)
    );

hang:
    while (1) { }
}
