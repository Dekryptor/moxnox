module;

#include <optional>
#include <source_location>
#include <string_view>
#include <mutex>
#include <span>
#include <atomic>
#include "ints.h"

module bsod;

#ifdef __aarch64__
import aarch64;
#elifdef __x86_64__
import apic;
#endif

import nt;
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
import cpu_block;
import mem;
import version;

using namespace std;

static atomic<unsigned int> bsod_count = 0;
static uint32_t bsod_cpu = 0xffffffff;

static void* bsod_malloc(size_t s) {
    return malloc_aligned<true>(s, 1);
}

static void bsod_free(void* ptr) {
    free_no_lock(ptr);
}

static void print_rip(chardev& p, uint64_t rip) {
    auto& cb = *get_cpu_block();
    uint64_t written;
    auto sp = span(cb.printk_scratch, PAGE_SIZE);

    auto img = get_kernel_image_from_address(rip);

    if (img) {
        dwarf_print_function(img->debug_info, img->debug_abbrev,
                             img->debug_rnglists, img->debug_addr,
                             img->debug_str, img->debug_str_offsets,
                             rip - (uintptr_t)img->base_address,
                             (uint64_t)img->base_address, sp, img->name, // FIXME - should be preferred base
                             bsod_malloc, bsod_free);

        sp = format(sp, " ");

        dwarf_print_line_no(img->debug_line, img->debug_line_str,
                            img->debug_str, img->debug_str_offsets,
                            rip, sp); // FIXME - adjust rip to preferred base

        sp = format(sp, "\n");
    } else
        sp = format(sp, "{:016x}\n", rip);

    p.write(string_view(cb.printk_scratch, (size_t)(sp.data() - cb.printk_scratch)),
            written);
}

extern "C++" optional<uint64_t> read_uint64_t(uint64_t addr) {
    // FIXME - do this safely
    return *(uint64_t*)addr;
}

static void print_stacktrace(const CONTEXT& context, chardev& p) {
    CONTEXT context2 = context;

    do {
#ifdef __aarch64__
        print_rip(p, context2.Pc);
#elifdef __x86_64__
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
        if (cb.cpu_no != bsod_cpu)
            halt_isr();
    }

    if (num_cpus > 1) {
#ifdef __aarch64__
        processors_left_to_halt = num_cpus - 1;

        if (cb.gicc) { // GICv2
            auto& GICD_SGIR = *(volatile uint32_t*)(gicd + 0xf00);
            ARM64_GICD_SGIR sgir;

            sgir.value = 0;
            sgir.TargetListFilter = 1; // send IPI to everybody else
            sgir.SGIINTID = HALT_SGI;

            GICD_SGIR = sgir.value;
        } else {
            ARM64_ICC_SGI1R sgir;

            sgir.value = 0;
            sgir.IRM = 1;
            sgir.INTID = HALT_SGI;

            __asm __volatile (
                "msr icc_sgi1r_el1, %x[res]"
                :
                : [res] "r" (sgir.value)
            );
        }

        while (processors_left_to_halt.load(memory_order_acquire) > 0) {
            __asm("yield");
        }
#elifdef __x86_64__
        processors_left_to_halt = num_cpus - 1;

        // send IPI to other cores to stop them

        auto icr1 = (volatile uint32_t*)&local_apic[0x300];

        *icr1 = 0xc0000 | halt_interrupt;

        while (processors_left_to_halt > 0) { }
#endif
    }

    // don't deadlock in malloc if we BSOD'd while holding pa_spinlock
    pa_spinlock.force_unlock();

    if (console_chardev) {
        static const char header[] = "BSOD";

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

        auto x_pos = (unsigned int)((console_cols - sizeof(header) + 1) / 2);

        for (unsigned int i = 0; i < x_pos; i++) {
            p.write(" ", written);
        }

        p.write("\e[1m", written);
        p.write(header, written);
        p.write("\e[22m", written);

        for (unsigned int i = 0; i < console_cols - x_pos - sizeof(header) + 1 - version.size(); i++) {
            p.write(" ", written);
        }

        p.write(version, written);
        p.write("\n\n", written);

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
    }

    shutdown_pipes();

    halt_isr();
}
