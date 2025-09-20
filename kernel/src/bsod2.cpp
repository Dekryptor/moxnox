#include <optional>
#include <source_location>
#include <string_view>
#include <mutex>
#include "spinlock.h"
#include "cpu_block.h"
#include "scheduler.h"
#include "ints.h"

#ifndef __aarch64__
import unwind;
import smp;
import image;
import apic;
#endif

import chardev;
import pipe;
import format;
import dwarf;
import config;
import console;
import interrupts;

using namespace std;

static spinlock bsod_lock;

#ifndef __aarch64__
static void print_rip(chardev& p, uint64_t rip) {
    auto& cb = *get_cpu_block();
    uint64_t written;
    auto sp = span(cb.printk_scratch, PAGE_SIZE);

    sp = format(sp, "{:016x}", rip);

    auto img = get_kernel_image_from_address(rip);

    if (img) {
        // FIXME - also get filename and line no. from DWARF

        sp = format(sp, " ({}!", img->name);

        auto func_start = dwarf_find_function(img->debug_info, img->debug_abbrev,
                                              img->debug_rnglists, img->debug_addr,
                                              img->debug_str, img->debug_str_offsets,
                                              rip, sp);

        if (func_start != 0) {
            if (rip != func_start)
                sp = format(sp, "+{:x}", rip - func_start);
        } else {
            // erase exclamation mark
            sp = span(sp.data() - 1, sp.size() + 1);

            sp = format(sp, "+{:x}", rip - (uintptr_t)img->base_address);
        }

        sp = format(sp, ")");
    }

    sp = format(sp, "\n");

    p.write(string_view(cb.printk_scratch, (size_t)(sp.data() - cb.printk_scratch)),
            written);
}

optional<uint64_t> read_uint64_t(uint64_t addr) {
    // FIXME - do this safely
    return *(uint64_t*)addr;
}

static bool do_unwind_context(CONTEXT& context) {
    auto img = get_kernel_image_from_address(context.Rip);
    if (!img)
        return false;

    return unwind_context(context, img->base_address, img->size, nullptr);
}

static void print_stacktrace(CONTEXT& context, chardev& p, unsigned int skip_callers) {
    // not interested in top entry (bsod function)
    if (!do_unwind_context(context))
        return;

    for (unsigned int i = 0; i < skip_callers; i++) {
        if (!do_unwind_context(context))
            return;
    }

    do {
        print_rip(p, context.Rip);

        if (!do_unwind_context(context))
            return;
    } while (true);
}
#endif

void __attribute__((noreturn)) _bsod(string_view msg, source_location sl,
                                     unsigned int skip_callers) {
    disable_preempt();

    // make sure that we don't get a deadlock if two BSODs at once
    enable_interrupts();

    lock_guard lock(bsod_lock);

    disable_interrupts();

#ifndef __aarch64__
    if (num_cpus > 1) {
        processors_left_to_halt = num_cpus - 1;

        // send IPI to other cores to stop them

        auto icr1 = (volatile uint32_t*)&local_apic[0x300];

        *icr1 = 0xc0000 | halt_interrupt;

        while (processors_left_to_halt > 0) { }
    }
#endif

    auto& p = static_cast<chardev&>(console_chardev.get());
    uint64_t written;

    // clear screen to blue and print message

    p.write(
        "\e[97;44m" // change to blue and white
        "\e[2J" // erase screen
        "\e[?25l" // hide cursor
        "\e[1;1H", // move to top left
        written
    );

    for (unsigned int i = 0; i < (console_cols / 2) - 2; i++) {
        p.write(" ", written);
    }

    p.write("\e[1mBSOD\e[22m\n\n", written);

    p.write(msg, written);
    p.write("\n", written);

    auto& cb = *get_cpu_block();
    auto sp = span(cb.printk_scratch, PAGE_SIZE);

    sp = format(sp, "{}:{}, {}, CPU {}\n",
                sl.file_name(), sl.line(), sl.function_name(), cb.cpu_no);

    p.write(string_view(cb.printk_scratch, (size_t)(sp.data() - cb.printk_scratch)),
            written);

    // FIXME - print context?

    CONTEXT context;

#ifndef __aarch64__
    __asm __volatile(
        "mov %0, rsp\n"
        "\tmov %1, rbp\n"
        "\tcall 0f\n"
        "\t0:\n"
        "\tpop rax\n"
        "\tmov %2, rax\n"
        : "=m" (context.Rsp), "=m" (context.Rbp), "=m" (context.Rip)
        :
        : "rax"
    );
#endif

    p.write("\n", written);

#ifndef __aarch64__
    print_stacktrace(context, p, skip_callers);
#endif

    p.flush(true);

    shutdown_pipes();

#ifndef __aarch64__
    __asm("call halt_isr");
    __builtin_unreachable();
#else
    while (true) { } // FIXME
#endif
}
