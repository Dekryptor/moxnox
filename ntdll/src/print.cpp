module;

#include <string_view>
#include <span>
#include <tuple>
#include <stdint.h>

export module print;

import nt;
import format;

using namespace std;

export
extern "C++"
void do_print(string_view s) {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    NtWriteFile(peb->ProcessParameters->StandardError, 0, nullptr, nullptr, &iosb, s.data(),
                (uint32_t)s.size(), nullptr, nullptr);
}

export
template<typename... Args>
void print(type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args) {
    char buf[4096];

    auto sp = span(buf, sizeof(buf));

    auto t = make_tuple(args...);

    sp = format_to(sp, fmt.sv, t);

    do_print(string_view(buf, (size_t)(sp.data() - buf)));
}
