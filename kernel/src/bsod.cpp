module;

#include <string_view>
#include <source_location>
#include <span>
#include <tuple>
#include <nt.h>
#include "cpu_block.h"

export module bsod;

import format;
import config;
import disable_interrupts;

using namespace std;

extern "C++" __attribute__((noreturn)) void _bsod(string_view msg, source_location sl,
                                                  unsigned int skip_callers,
                                                  CONTEXT* captured_context);

export bool in_bsod = false;

export
__attribute__((noreturn))
void bsod(string_view msg, source_location sl = source_location::current(),
          unsigned int skip_callers = 1, CONTEXT* captured_context = nullptr) {
    in_bsod = true;

    _bsod(msg, sl, skip_callers, captured_context);
}

export
template<typename... Args>
struct bsod_format {
    __attribute__((noreturn))
    bsod_format(type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args,
                const source_location& sl = source_location::current()) {
        disable_preempt();

        auto& cb = *get_cpu_block();

        auto sp = span(cb.printk_scratch, PAGE_SIZE);

        sp = format_to(sp, fmt.sv, make_tuple(args...));

        bsod(string_view(cb.printk_scratch, (size_t)(sp.data() - cb.printk_scratch)),
             sl, 2);

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
