module;

#include <string_view>
#include <tuple>
#include <span>
#include <stdint.h>

export module print;

import nt;
import format;

using namespace std;

export
template<typename... Args>
void print(type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args) {
    char buf[4096];
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    auto sp = span(buf, sizeof(buf));

    auto t = make_tuple(args...);

    sp = format_to(sp, fmt.sv, t);

    auto sv = string_view(buf, (size_t)(sp.data() - buf));

    NtWriteFile(peb->ProcessParameters->StandardOutput, 0, nullptr, nullptr,
                &iosb, sv.data(), (uint32_t)sv.size(), nullptr, nullptr);
}
