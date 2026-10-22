// SPDX-License-Identifier: MIT
module;

#include <string_view>
#include <tuple>
#include <span>
#include <stddef.h>
#include <stdint.h>

export module print;

import nt;
import format;

using namespace std;

export
template<typename... Args>
void print(type_identity_t<checker<type_identity_t<Args>...>> fmt, Args&&... args) {
    char buf[4096];
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    auto sp = span(buf, sizeof(buf));

    sp = format_to(sp, fmt.sv, forward_as_tuple(std::forward<Args>(args)...));

    auto sv = string_view(buf, (size_t)(sp.data() - buf));

    while (true) {
        auto status = NtWriteFile(peb->ProcessParameters->StandardOutput, 0,
                                  nullptr, nullptr, &iosb, sv.data(),
                                  (uint32_t)sv.size(), nullptr, nullptr);

        if (status == nt_status::USER_APC)
            continue;
        else if (!success(status) || iosb.Information == sv.size())
            return;

        sv = string_view(sv.data() + iosb.Information, sv.size() - iosb.Information);
    }
}

export
template<typename... Args>
void print_stderr(type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args) {
    char buf[4096];
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    auto sp = span(buf, sizeof(buf));

    sp = format_to(sp, fmt.sv, forward_as_tuple(std::forward<Args>(args)...));

    auto sv = string_view(buf, (size_t)(sp.data() - buf));

    while (true) {
        auto status = NtWriteFile(peb->ProcessParameters->StandardError, 0,
                                  nullptr, nullptr, &iosb, sv.data(),
                                  (uint32_t)sv.size(), nullptr, nullptr);

        if (status == nt_status::USER_APC)
            continue;
        else if (!success(status) || iosb.Information == sv.size())
            return;

        sv = string_view(sv.data() + iosb.Information, sv.size() - iosb.Information);
    }
}
