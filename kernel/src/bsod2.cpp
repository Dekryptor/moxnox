#include <optional>
#include <source_location>
#include <string_view>
#include <mutex>
#include <span>
#include "spinlock.h"
#include "cpu_block.h"
#include "ints.h"

#ifndef __aarch64__
import apic;
#endif

import chardev;
import pipe;
import format;
import dwarf;
import config;
import console;
import interrupts;
import disable_interrupts;
import scheduler;
import kernel_images;
import smp;
import unwind;

using namespace std;

static atomic<unsigned int> bsod_count = 0;
static uint32_t bsod_cpu = 0xffffffff;

static void print_rip(chardev& p, uint64_t rip) {
    auto& cb = *get_cpu_block();
    uint64_t written;
    auto sp = span(cb.printk_scratch, PAGE_SIZE);

    auto img = get_kernel_image_from_address(rip);

    if (img) {
        // FIXME - also get filename and line no. from DWARF

        dwarf_print_function(img->debug_info, img->debug_abbrev,
                             img->debug_rnglists, img->debug_addr,
                             img->debug_str, img->debug_str_offsets,
                             rip - (uintptr_t)img->base_address,
                             (uint64_t)img->base_address, sp, img->name); // FIXME - should be preferred base
    } else
        sp = format(sp, "{:016x}\n", rip);

    p.write(string_view(cb.printk_scratch, (size_t)(sp.data() - cb.printk_scratch)),
            written);
}

optional<uint64_t> read_uint64_t(uint64_t addr) {
    // FIXME - do this safely
    return *(uint64_t*)addr;
}

static bool do_unwind_context(CONTEXT& context) {
#ifdef __aarch64__
    auto img = get_kernel_image_from_address(context.Pc);
#else
    auto img = get_kernel_image_from_address(context.Rip);
#endif
    if (!img)
        return false;

    return unwind_context(context, img->base_address, img->size);
}

static void print_stacktrace(const CONTEXT& context, chardev& p) {
    CONTEXT context2 = context;

    do {
#ifdef __aarch64__
        print_rip(p, context2.Pc);
#else
        print_rip(p, context2.Rip);
#endif

        if (!do_unwind_context(context2))
            return;
    } while (true);
}

void __attribute__((noreturn)) _bsod(string_view msg, source_location sl,
                                     const CONTEXT& captured_context) {
    disable_interrupts();

    auto& cb = *get_cpu_block();

    if (bsod_count.fetch_add(1) == 0)
        bsod_cpu = cb.cpu_no;
    else {
        if (cb.cpu_no != bsod_cpu) {
#ifdef __aarch64__
            while (true) { } // FIXME
#else
            __asm("call halt_isr");
            __builtin_unreachable();
#endif
        }
    }

    if (num_cpus > 1) {
#ifdef __aarch64__
        processors_left_to_halt = num_cpus - 1;

        auto& GICD_SGIR = *(volatile uint32_t*)(gicd + 0xf00);
        ARM64_GICD_SGIR sgir;

        sgir.value = 0;
        sgir.TargetListFilter = 1; // send IPI to everybody else
        sgir.SGIINTID = HALT_SGI;

        GICD_SGIR = sgir.value;

        while (processors_left_to_halt.load(memory_order_acquire) > 0) {
            __asm("yield");
        }
#else
        processors_left_to_halt = num_cpus - 1;

        // send IPI to other cores to stop them

        auto icr1 = (volatile uint32_t*)&local_apic[0x300];

        *icr1 = 0xc0000 | halt_interrupt;

        while (processors_left_to_halt > 0) { }
#endif
    }

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

    auto sp = span(cb.printk_scratch, PAGE_SIZE);

    sp = format(sp, "{}:{}, {}, CPU {}\n",
                sl.file_name(), sl.line(), sl.function_name(), cb.cpu_no);

    p.write(string_view(cb.printk_scratch, (size_t)(sp.data() - cb.printk_scratch)),
            written);

    // FIXME - print context?

    p.write("\n", written);

    print_stacktrace(captured_context, p);

    p.flush(true);

    shutdown_pipes();

#ifdef __aarch64__
    halt_isr();
#else
    __asm("call halt_isr");
#endif

    __builtin_unreachable();
}
