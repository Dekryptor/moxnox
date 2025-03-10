module;

#include <nt.h>
#include <string_view>
#include <span>

export module print;

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

    sp = format_to(sp, fmt.sv, args...);

    auto sv = string_view(buf, (size_t)(sp.data() - buf));

    NtWriteFile(peb->ProcessParameters->StandardOutput, 0, nullptr, nullptr,
                &iosb, sv.data(), (uint32_t)sv.size(), nullptr, nullptr);
}
