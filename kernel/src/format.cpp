module;

#include <span>
#include <string_view>
#include <source_location>
#include <stdint.h>
#include <nt.h>
#include "cpu_block.h"

export module format;

using namespace std;

extern "C++" void (*_printk)(string_view s);
extern "C++" void disable_preempt();
extern "C++" void enable_preempt();
extern "C++" __attribute__((noreturn)) void bsod(string_view msg, source_location sl,
                                                 bool skip_caller);

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

    uint64_t place = 10000000000000000000ull;

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

template<typename T>
consteval bool valid_spec(string_view fmt) {
    return false;
}

span<char> do_format_arg(span<char> sp, string_view fmt, integral auto arg) {
    if (fmt.empty())
        return sp_itoa(sp, arg);

    if (fmt.size() >= 2 && fmt.front() == ':' && fmt.back() == 'x') {
        unsigned int padding = 0;

        for (size_t i = 1; i < fmt.size() - 1; i++) {
            padding *= 10;
            padding += fmt[i] - '0';
        }

        // FIXME - should be padded with spaces if first digit not 0

        return sp_itoa_hex(sp, arg, padding);
    }

    return sp;
}

template<typename T>
requires (integral<T>)
consteval bool valid_spec(string_view fmt) {
    if (fmt.empty())
        return true;

    if (fmt.size() >= 2 && fmt.front() == ':' && fmt.back() == 'x') {
        for (size_t i = 1; i < fmt.size() - 1; i++) {
            if (fmt[i] < '0' || fmt[i] > '9')
                return false;
        }

        return true;
    }

    return false;
}

span<char> do_format_arg(span<char> sp, string_view fmt, nt_status arg) {
    // FIXME - format nt_status to string?

    return do_format_arg(sp, fmt, (uint32_t)arg);
}

template<>
consteval bool valid_spec<nt_status>(string_view fmt) {
    return valid_spec<unsigned int>(fmt);
}

span<char> do_format_arg(span<char> sp, string_view, string_view arg) {
    return sp_strcpy(sp, arg);
}

template<>
consteval bool valid_spec<string_view>(string_view fmt) {
    return fmt.empty();
}

template<>
consteval bool valid_spec<char*>(string_view fmt) {
    return valid_spec<string_view>(fmt);
}

template<>
consteval bool valid_spec<const char*>(string_view fmt) {
    return valid_spec<string_view>(fmt);
}

span<char> do_format_arg(span<char> sp, string_view, const AMD64_CONTEXT& ctx) {
    sp = sp_strcpy(sp, "rax=");
    sp = sp_itoa_hex(sp, ctx.Rax, 16);
    sp = sp_strcpy(sp, " rcx=");
    sp = sp_itoa_hex(sp, ctx.Rcx, 16);
    sp = sp_strcpy(sp, " rdx=");
    sp = sp_itoa_hex(sp, ctx.Rdx, 16);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "rbx=");
    sp = sp_itoa_hex(sp, ctx.Rbx, 16);
    sp = sp_strcpy(sp, " rsp=");
    sp = sp_itoa_hex(sp, ctx.Rsp, 16);
    sp = sp_strcpy(sp, " rbp=");
    sp = sp_itoa_hex(sp, ctx.Rbp, 16);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "rsi=");
    sp = sp_itoa_hex(sp, ctx.Rsi, 16);
    sp = sp_strcpy(sp, " rdi=");
    sp = sp_itoa_hex(sp, ctx.Rdi, 16);
    sp = sp_strcpy(sp, "  r8=");
    sp = sp_itoa_hex(sp, ctx.R8, 16);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, " r9=");
    sp = sp_itoa_hex(sp, ctx.R9, 16);
    sp = sp_strcpy(sp, " r10=");
    sp = sp_itoa_hex(sp, ctx.R10, 16);
    sp = sp_strcpy(sp, " r11=");
    sp = sp_itoa_hex(sp, ctx.R11, 16);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "r12=");
    sp = sp_itoa_hex(sp, ctx.R12, 16);
    sp = sp_strcpy(sp, " r13=");
    sp = sp_itoa_hex(sp, ctx.R13, 16);
    sp = sp_strcpy(sp, " r14=");
    sp = sp_itoa_hex(sp, ctx.R14, 16);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "r15=");
    sp = sp_itoa_hex(sp, ctx.R15, 16);
    sp = sp_strcpy(sp, " rip=");
    sp = sp_itoa_hex(sp, ctx.Rip, 16);
    sp = sp_strcpy(sp, " eflags=");
    sp = sp_itoa_hex(sp, ctx.EFlags, 8);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "cs=");
    sp = sp_itoa_hex(sp, ctx.SegCs, 4);
    sp = sp_strcpy(sp, " ds=");
    sp = sp_itoa_hex(sp, ctx.SegDs, 4);
    sp = sp_strcpy(sp, " es=");
    sp = sp_itoa_hex(sp, ctx.SegEs, 4);
    sp = sp_strcpy(sp, " fs=");
    sp = sp_itoa_hex(sp, ctx.SegFs, 4);
    sp = sp_strcpy(sp, " gs=");
    sp = sp_itoa_hex(sp, ctx.SegGs, 4);
    sp = sp_strcpy(sp, " ss=");
    sp = sp_itoa_hex(sp, ctx.SegSs, 4);
    sp = sp_strcpy(sp, "\n");

    return sp;
}

template<>
consteval bool valid_spec<AMD64_CONTEXT>(string_view fmt) {
    return fmt.empty();
}

template<typename T, typename... Args>
span<char> format_to(span<char> sp, string_view fmt, const T& arg, Args&&... args) {
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

void checker_failure();

template<typename T>
consteval void check_spec(string_view sp) {
    if (!valid_spec<remove_cvref_t<T>>(sp))
        checker_failure();
}

template<typename T, typename... Args>
consteval void check_specs(span<string_view, sizeof...(Args) + 1> sp,
                           tuple<T, Args...>*) {
    check_spec<T>(sp[0]);
    // FIXME

    if constexpr (sizeof...(Args) > 0)
        check_specs(span<string_view, sizeof...(Args)>(sp.data() + 1, sp.size() - 1), (tuple<Args...>*)nullptr);
}

template<typename... Args>
struct checker {
    consteval checker(const char* q) : sv(q) {
        unsigned int num_entries = 0;

        // FIXME - {{ and }} escaping

        auto s = sv;

        while (!s.empty()) {
            if (s.front() == '{') {
                num_entries++;

                auto end = s.find('}');
                if (end == string_view::npos)
                    checker_failure();

                s = s.substr(end + 1);
                continue;
            }

            s = s.substr(1);
        }

        if (num_entries > sizeof...(Args))
            checker_failure();

        array<string_view, sizeof...(Args)> spec;

        s = sv;

        unsigned int pos = 0;

        while (!s.empty()) {
            if (s.front() == '{') {
                auto end = s.find('}');

                spec[pos] = s.substr(1, end - 1);

                s = s.substr(end + 1);
                pos++;
                continue;
            }

            s = s.substr(1);
        }

        if constexpr (sizeof...(Args) > 0)
            check_specs(spec, (tuple<Args...>*)nullptr);
    }

    string_view sv;
};

export
template<typename... Args>
span<char> format(span<char> sp, type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args) {
    return format_to(sp, fmt.sv, args...);
}

export
template<typename... Args>
void print(type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args) {
    disable_preempt();

    auto& cb = *get_cpu_block();

    auto sp = span(cb.printk_scratch, PAGE_SIZE);

    sp = format_to(sp, fmt.sv, args...);

    _printk(string_view(cb.printk_scratch, sp.data() - cb.printk_scratch));

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

        bsod(string_view(cb.printk_scratch, sp.data() - cb.printk_scratch), sl,
             true);

        enable_preempt();
    }
};

export
template<typename... Args>
bsod_format(type_identity_t<checker<type_identity_t<Args>...>>, Args&&...) -> bsod_format<Args...>;
