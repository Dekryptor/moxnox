module;

#include <span>
#include <string_view>
#include <stdint.h>

export module format;

using namespace std;

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

span<char> do_format_arg(span<char> sp, integral auto arg) {
    return sp_itoa(sp, arg);
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

    sp = do_format_arg(sp, arg); // FIXME - pass through what's between curly brackets

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

using func_type = void(string_view s);

export
template<typename... Args>
void print(func_type print_func, string_view fmt, Args&&... args) {
    char s[50]; // FIXME
    auto sp = span(s, sizeof(s));

    sp = format_to(sp, fmt, args...);

    print_func(string_view(s, sp.data() - s));
}
