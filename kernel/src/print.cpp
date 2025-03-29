module;

#include <span>
#include <string_view>
#include <tuple>
#include <nt.h>
#include "cpu_block.h"

export module print;

import format;
import config;

using namespace std;

extern "C++" void (*_printk)(string_view s);
extern "C++" void disable_preempt();
extern "C++" void enable_preempt();

export
template<typename... Args>
void print(type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args) {
    disable_preempt();

    auto& cb = *get_cpu_block();

    auto sp = span(cb.printk_scratch, PAGE_SIZE);

    sp = format_to(sp, fmt.sv, make_tuple(args...));

    _printk(string_view(cb.printk_scratch, (size_t)(sp.data() - cb.printk_scratch)));

    enable_preempt();
}

export
extern "C++"
void do_print(string_view sv) {
    _printk(sv);
}
