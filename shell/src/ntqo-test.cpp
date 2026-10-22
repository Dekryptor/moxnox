// SPDX-License-Identifier: GPL-2.0-only
#include <stdio.h>
#include <stdint.h>
#include <string_view>

import nt;
import print;

using namespace std;

int main() {
    uint8_t buf[1024];
    uint32_t retlen = 0;

    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;

    auto status = NtQueryObject(peb.ProcessParameters->StandardOutput,
                                OBJECT_INFORMATION_CLASS::NameInformation,
                                buf, sizeof(buf), &retlen);
    if (!success(status)) {
        print("NtQueryObject returned {}\n", status);
        return 1;
    }

    auto& name = ((OBJECT_NAME_INFORMATION*)buf)->Name;

    print("StandardOutput name: \"{}\"\n", u16string_view(name.Buffer, name.Length / sizeof(char16_t)));

    return 0;
}
