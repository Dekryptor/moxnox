module;

#include <string_view>
#include <source_location>
#include <span>
#include <tuple>

export module bsod;

import nt;
import format;
import config;
import disable_interrupts;
import unwind;
import kernel_images;
import cpu_block;

using namespace std;

export __attribute__((noreturn)) void _bsod(string_view msg, source_location sl,
                                            const CONTEXT& captured_context);

export bool in_bsod = false;

bool do_unwind_context(CONTEXT& context) {
#ifdef __aarch64__
    auto img = get_kernel_image_from_address(context.Pc);
#elifdef __x86_64__
    auto img = get_kernel_image_from_address(context.Rip);
#endif
    if (!img)
        return false;

    return unwind_context(context, img->base_address, img->size);
}

inline __attribute__((always_inline)) void capture_context(CONTEXT& ctx) {
    // FIXME - all regs?

#ifdef __aarch64__
    __asm __volatile (
        "mov x0, lr\n"
        "bl 0f\n"
        "0:\n"
        "mov %0, lr\n"
        "mov lr, x0\n"
        "mov %1, lr\n"
        "mov %2, sp\n"
        "mov %3, fp\n"
        : "=r" (ctx.Pc), "=r" (ctx.Lr), "=r" (ctx.Sp), "=r" (ctx.Fp)
        :
        : "x0"
    );
#elifdef __x86_64__
    __asm __volatile(
        "mov %0, rsp\n"
        "\tmov %1, rbp\n"
        "\tcall 0f\n"
        "\t0:\n"
        "\tpop rax\n"
        "\tmov %2, rax\n"
        : "=m" (ctx.Rsp), "=m" (ctx.Rbp), "=m" (ctx.Rip)
        :
        : "rax"
    );
#endif
}

export
__attribute__((noreturn))
void bsod(string_view msg, source_location sl = source_location::current(),
          CONTEXT* captured_context = nullptr) {
    CONTEXT this_context;

    in_bsod = true;

    if (!captured_context) {
        capture_context(this_context);
        do_unwind_context(this_context);
    }

    _bsod(msg, sl, captured_context ? *captured_context : this_context);
}

export
template<typename... Args>
struct bsod_format {
    __attribute__((noreturn))
    bsod_format(type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args,
                const source_location& sl = source_location::current()) {
        CONTEXT this_context;

        disable_preempt();

        auto& cb = *get_cpu_block();

        auto sp = span(cb.printk_scratch, PAGE_SIZE);

        sp = format_to(sp, fmt.sv, make_tuple(args...));

        in_bsod = true;

        capture_context(this_context);
        do_unwind_context(this_context);

        _bsod(string_view(cb.printk_scratch, (size_t)(sp.data() - cb.printk_scratch)),
              sl, this_context);

        enable_preempt();
    }
};

export
template<typename... Args>
bsod_format(type_identity_t<checker<type_identity_t<Args>...>>, Args&&...) -> bsod_format<Args...>;

export void _assert(const char* msg, const char* file, unsigned int line) {
    bsod_format("assertion failed at {}:{}: {}", file, line, msg);
    __builtin_unreachable();
}

export extern "C" void abort() {
    bsod("abort() called");
}
