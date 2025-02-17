module;

#include <span>
#include <string_view>
#include <stdint.h>
#include <nt.h>
#include "cpu_block.h"

export module format;

using namespace std;

extern "C++" void safe_print(string_view s);
extern "C++" decltype(&safe_print) _printk;
extern "C++" void disable_preempt();
extern "C++" void enable_preempt();

#define PAGE_SIZE 4096 // FIXME - only define in one place

span<char> sp_strcpy(span<char> dest, string_view src) {
    auto to_copy = min(src.size(), dest.size());

    memcpy(dest.data(), src.data(), to_copy);

    return span(dest.data() + to_copy, dest.size() - to_copy);
}

span<char> sp_itoa(span<char> dest, uint64_t num) {
    bool first = true;

    if (dest.empty())
        return dest;

    if (num == 0) {
        dest[0] = '0';
        return span(dest.data() + 1, dest.size() - 1);
    }

    uint64_t place = 1000000000;

    while (place > 0) {
        if (!first || num >= place) {
            uint64_t digit = num / place;

            dest[0] = (char)(digit + '0');

            dest = span(dest.data() + 1, dest.size() - 1);
            first = false;

            if (dest.empty())
                return dest;

            num %= place;
        }

        place /= 10;
    }

    return dest;
}

span<char> sp_itoa_hex(span<char> dest, uint64_t num, unsigned int padding = 0) {
    bool first = true;

    if (dest.empty())
        return dest;

    if (num == 0 && padding == 0) {
        dest[0] = '0';
        return span(dest.data() + 1, dest.size() - 1);
    }

    for (unsigned int i = 0; i < 16; i++) {
        if (!first || padding == 16 - i || num & 0xf000000000000000) {
            uint64_t n = num >> 60;

            if (n <= 9)
                dest[0] = (char)(n + '0');
            else
                dest[0] = (char)(n - 0xa + 'a');

            dest = span(dest.data() + 1, dest.size() - 1);
            first = false;

            if (dest.empty())
                return dest;
        }

        num <<= 4;
    }

    return dest;
}

span<char> do_format_arg(span<char> sp, string_view fmt, integral auto arg) {
    if (fmt == ":x")
        return sp_itoa_hex(sp, arg);

    return sp_itoa(sp, arg);
}

span<char> do_format_arg(span<char> sp, string_view, string_view arg) {
    return sp_strcpy(sp, arg);
}

template<typename T, typename... Args>
span<char> format_to(span<char> sp, string_view fmt, const T& arg, Args&&... args) {
    // FIXME - consteval checking

    // FIXME - {{ and }} escaping

    auto brack = fmt.find('{');

    if (brack == string_view::npos)
        return sp_strcpy(sp, fmt);

    if (brack != 0) {
        sp = sp_strcpy(sp, string_view(fmt.data(), brack));
        fmt = string_view(fmt.data() + brack, fmt.size() - brack);
    }

    auto close = fmt.find('}');

    if (close == string_view::npos)
        return sp;

    sp = do_format_arg(sp, string_view(fmt.data() + 1, close - 1), arg);

    fmt = string_view(fmt.data() + close + 1, fmt.size() - close - 1);

    if (fmt.empty())
        return sp;

    if constexpr (sizeof...(args) > 0)
        return format_to(sp, fmt, args...);
    else
        return sp_strcpy(sp, fmt);
}

span<char> format_to(span<char> sp, string_view fmt) {
    return sp_strcpy(sp, fmt);
}

export
template<typename... Args>
void print(string_view fmt, Args&&... args) {
    disable_preempt();

    auto& cb = *get_cpu_block();

    auto sp = span(cb.printk_scratch, PAGE_SIZE);

    sp = format_to(sp, fmt, args...);

    _printk(string_view(cb.printk_scratch, sp.data() - cb.printk_scratch));

    enable_preempt();
}
