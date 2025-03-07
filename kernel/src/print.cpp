module;

#include <span>
#include <string_view>
#include <source_location>
#include <stdint.h>
#include <nt.h>
#include "cpu_block.h"

export module print;

import format;

#define PAGE_SIZE 4096 // FIXME - only define in one place

using namespace std;

extern "C++" void (*_printk)(string_view s);
extern "C++" void disable_preempt();
extern "C++" void enable_preempt();
extern "C++" __attribute__((noreturn)) void bsod(string_view msg, source_location sl,
                                                 bool skip_caller);

export
template<typename... Args>
void print(type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args) {
    disable_preempt();

    auto& cb = *get_cpu_block();

    auto sp = span(cb.printk_scratch, PAGE_SIZE);

    sp = format_to(sp, fmt.sv, args...);

    _printk(string_view(cb.printk_scratch, (size_t)(sp.data() - cb.printk_scratch)));

    enable_preempt();
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

        sp = format_to(sp, fmt.sv, args...);

        bsod(string_view(cb.printk_scratch, (size_t)(sp.data() - cb.printk_scratch)), sl,
             true);

        enable_preempt();
    }
};

export
template<typename... Args>
bsod_format(type_identity_t<checker<type_identity_t<Args>...>>, Args&&...) -> bsod_format<Args...>;
