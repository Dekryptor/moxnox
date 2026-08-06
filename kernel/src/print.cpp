module;

#include <span>
#include <string_view>
#include <tuple>

export module print;

import format;
import config;
import disable_interrupts;
import cpu_block;

using namespace std;

extern "C++" void (*_printk)(string_view s);

export
template<typename... Args>
void print(type_identity_t<checker<type_identity_t<Args>...>> fmt, Args&&... args) {
    disable_preempt();

    auto& cb = *get_cpu_block();

    span<char> sp;

    if (cb.deferred_print > 0)
        sp = span(cb.printk_scratch + cb.deferred_print_size, PAGE_SIZE - cb.deferred_print_size);
    else
        sp = span(cb.printk_scratch, PAGE_SIZE);

    sp = format_to(sp, fmt.sv, forward_as_tuple(std::forward<Args>(args)...));

    if (cb.deferred_print > 0)
        cb.deferred_print_size = sp.data() - cb.printk_scratch;
    else
        _printk(string_view(cb.printk_scratch, (size_t)(sp.data() - cb.printk_scratch)));

    enable_preempt();
}

export
extern "C++"
void do_print(string_view sv) {
    _printk(sv);
}

export void start_deferred_print() {
    disable_preempt();

    auto& cb = *get_cpu_block();

    if (cb.deferred_print == 0)
        cb.deferred_print_size = 0;

    cb.deferred_print++;
}

export void end_deferred_print() {
    auto& cb = *get_cpu_block();

    cb.deferred_print--;

    if (cb.deferred_print == 0) {
        _printk(string_view(cb.printk_scratch, cb.deferred_print_size));
        cb.deferred_print_size = 0;
    }

    enable_preempt();
}
