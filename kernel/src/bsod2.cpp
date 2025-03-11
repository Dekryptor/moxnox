#include "kernel.h"
#include "spinlock.h"
#include "cpu_block.h"
#include "objects.h"
#include "pipe.h"
#include "handle.h"
#include <pe.h>

import format;
import print;
import dwarf;
import bsod;
import unwind;

using namespace std;

// console.cpp (FIXME)
extern object_ref console_pipe;
extern unsigned int console_cols;

static spinlock bsod_lock;

static kernel_image* get_kernel_image_from_address(uint64_t addr) {
    auto le = kernel_images.next;

    while (le != &kernel_images) {
        auto& ent = containing_record<kernel_image>(le);

        if (addr >= (uintptr_t)ent.base_address && addr < (uintptr_t)ent.base_address + ent.size)
            return &ent;

        le = le->next;
    }

    return nullptr;
}

static void print_rip(pipe& p, uint64_t rip) {
    auto& cb = *get_cpu_block();
    uint64_t written;
    auto sp = span(cb.printk_scratch, PAGE_SIZE);

    sp = format(sp, "{:16x}", rip);

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

static bool do_unwind_context(CONTEXT& context) {
    auto img = get_kernel_image_from_address(context.Rip);
    if (!img)
        return false;

    return unwind_context(context, img->base_address, img->size);
}

static void print_stacktrace(CONTEXT& context, pipe& p, unsigned int skip_callers) {
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

void __attribute__((noreturn)) _bsod(string_view msg, source_location sl,
                                     unsigned int skip_callers) {
    __asm("cli");

    lock_guard lock(bsod_lock);

    if (num_cpus > 1) {
        processors_left_to_halt = num_cpus - 1;

        // send IPI to other cores to stop them

        auto icr1 = (volatile uint32_t*)&local_apic[0x300];

        *icr1 = 0xc0000 | halt_interrupt;

        while (processors_left_to_halt > 0) { }
    }

    auto& p = static_cast<pipe&>(console_pipe.get());
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

    sp = format(sp, "{}:{}, {}, CPU {:x}\n",
                sl.file_name(), sl.line(), sl.function_name(), cb.cpu_no);

    p.write(string_view(cb.printk_scratch, (size_t)(sp.data() - cb.printk_scratch)),
            written);

    // FIXME - print context?

    CONTEXT context;

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

    p.write("\n", written);

    print_stacktrace(context, p, skip_callers);

    p.flush(true);

    shutdown_pipes();

    __asm("call halt_isr");
    __builtin_unreachable();
}
