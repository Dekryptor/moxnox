#include <span>
#include <atomic>
#include <string_view>
#include <optional>
#include "context_offsets.h"
#include "timer-asm.h"

#ifdef __aarch64__
import psci;
#elifdef __x86_64__
import serial;
#endif

import nt;
import config;
import format;
import bsod;
import print;
import cmdline;
import mem;
import thread;
import console;
import acpi;
import interrupts;
import timer;
import disable_interrupts;
import scheduler;
import list;
import chardev;
import evdev;
import unicode;
import null;
import vfs;
import section;
import image;
import keyboard;
import process;
import syscalls;
import smp;
import object_ref;
import cpu_block;
import pci;
import loaderblock;
import efi;
import debug;
import crt;
import version;
import objects;
import handle;

using namespace std;

extern uint64_t __CTOR_LIST__;

static char cmdline_arr[255];
static loader_block_framebuffer lb_fb;
#ifdef __x86_64__
static loader_block_text_console lb_tc;
#endif

void (*_printk)(string_view s) = very_early_print;

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

    auto ref = create_process("/bin/init.exe", "/", u16string_view(init_cmdline, init_cmdline_len),
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

void main_thread(void* ctx) {
    auto& lb = *(loader_block*)ctx;

    print("\e[2J\e[1;1H");  // erase screen, move to top left

    acpi_early_init_done = true;

    setup_folios();

    if (!init_keyboard_pipe())
        print("init_keyboard_pipe failed\n");

    init_acpi();
    find_acpi_devices();

    init_pci(true);

    init_debug();

#ifdef __aarch64__
    if (!init_console(lb_fb))
        bsod("init_console failed\n");
#elifdef __x86_64__
    if (!init_console(lb_fb, lb_tc))
        bsod("init_console failed\n");
#endif

    _printk = safe_print;

    if ((console_type == console_type::boot_framebuffer || console_type == console_type::other_framebuffer) && !init_fb_console())
        print("init_fb_console failed\n");

    if (lb.SetVirtualAddressMap) {
        if (!map_uefi(lb))
            bsod("map_uefi failed");
    }

    memcpy(version_buf, lb.version, sizeof(lb.version));
    version = string_view(version_buf);

    free_lower_half_mappings();
    free_bootloader_memory();

    if (GetTime)
        get_time_from_efi();

#ifdef __x86_64__
    // if no ACPI, look for serial port at its traditional address
    if (rsdp_pa == 0)
        init_serial(0x3f8, 4, "");
#endif

    print("found {} of memory\n", byte_size(num_ram_pages << PAGE_SHIFT));

    if (version[0] != 0)
        print("{}\n", version);

    keywait();

    init_pci(false);

#ifdef __x86_64__
    init_syscalls();
#endif

#ifdef __aarch64__
    init_psci();
#endif

    if (!init_smp()) {
        auto bsp_id = current_cpu_id();

        cpu_ids = (decltype(bsp_id)*)malloc(1);
        if (!cpu_ids) {
            print("malloc failed\n");
            return;
        }

        cpu_ids[0] = bsp_id;
    }

    protect_kernel_pages();

    if (!create_dev_null())
        print("create_dev_null failed\n");

    if (!mount_root())
        bsod("mount_root failed");

    if (!mount_devfs())
        bsod("mount_devfs failed");

    load_ntdll();
    create_time_page();

    auto init = create_init_process();

    if (!init)
        bsod("Could not start init process.");

    static_cast<process&>(init.get()).resume();

    wait_for_object(init.get(), false);

    auto& init_process = static_cast<process&>(init.get());

    bsod_format("init process died (exit code {})", init_process.exit_status);
}

extern "C"
void startup(loader_block* lb) {
    span<const uint8_t> debug_info, debug_abbrev, debug_rnglists, debug_str,
                        debug_addr, debug_str_offsets, debug_line,
                        debug_line_str;

    // FIXME - should restart if anything fails before we can show error message

#ifdef __x86_64__
    if (lb->text_mode)
        memcpy(&lb_tc, &lb->text_console, sizeof(lb_tc));
    else
#endif
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

#ifdef __x86_64__
    if (lb->text_mode)
        console_type = console_type::text;
    else
        console_type = console_type::boot_framebuffer;
#endif

    debug_info = span((uint8_t*)(uintptr_t)lb->debug_info_ptr, lb->debug_info_size);
    debug_abbrev = span((uint8_t*)(uintptr_t)lb->debug_abbrev_ptr, lb->debug_abbrev_size);
    debug_rnglists = span((uint8_t*)(uintptr_t)lb->debug_rnglists_ptr, lb->debug_rnglists_size);
    debug_str = span((uint8_t*)(uintptr_t)lb->debug_str_ptr, lb->debug_str_size);
    debug_addr = span((uint8_t*)(uintptr_t)lb->debug_addr_ptr, lb->debug_addr_size);
    debug_str_offsets = span((uint8_t*)(uintptr_t)lb->debug_str_offsets_ptr, lb->debug_str_offsets_size);
    debug_line = span((uint8_t*)(uintptr_t)lb->debug_line_ptr, lb->debug_line_size);
    debug_line_str = span((uint8_t*)(uintptr_t)lb->debug_line_str_ptr, lb->debug_line_str_size);

    rsdp_pa = lb->rsdp_pa;

    if (!init_memory(*lb))
        goto hang;

    call_global_constructors();

    if (!copy_map_ptr(*lb))
        goto hang;

    init_images_list(debug_info, debug_abbrev, debug_rnglists, debug_str,
                     debug_addr, debug_str_offsets, debug_line, debug_line_str);

    if (!init_vablock())
        goto hang;

#ifdef __aarch64__
    if (!init_cpu_block(0))
        goto hang;
#elifdef __x86_64__
    if (!init_cpu_block(current_cpu_id()))
        goto hang;
#endif

    init_acpi_early();

    if (!init_interrupts())
        goto hang;

#ifdef __x86_64__
    if (!init_apic())
        goto hang;
#endif

    enable_interrupts();

    _printk = early_print;

    if (!add_kernel_thread(main_thread, lb))
        goto hang;

    // FIXME - C++ exceptions

    if (!init_timer()) // leaves interrupts disabled
        goto hang;

    acquire_sync_lock();

#ifdef __aarch64__
    __asm __volatile ( // doesn't return
        "add x0, x18, #%0\n\t"
        "ldr x0, [x0]\n\t"
        "mov sp, x0\n\t"
        "b scheduler"
        :
        : "i" (offsetof(cpu_block, scheduler_stack))
    );
#elifdef __x86_64__
    __asm __volatile ( // doesn't return
        "mov rsp, gs:%0\n"
        "jmp scheduler\n"
        :
        : "i" (offsetof(cpu_block, scheduler_stack))
    );
#endif

hang:
    while (1) { }
}
