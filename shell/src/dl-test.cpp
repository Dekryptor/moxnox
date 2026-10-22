// SPDX-License-Identifier: GPL-2.0-only
#include <string_view>
#include <stdint.h>

import nt;
import ntdll;
import print;

using namespace std;

using func_proto = char* (*)();

int main() {
    nt_status status;
    UNICODE_STRING us;
    ANSI_STRING as;
    void* dll;
    void* addr;

    static const u16string_view libcurl = u"libcurl.dll";
    static const string_view funcname = "curl_version";

    us.Buffer = (char16_t*)libcurl.data();
    us.Length = us.MaximumLength = (uint16_t)(libcurl.size() * sizeof(char16_t));

    status = LdrLoadDll(nullptr, nullptr, &us, &dll);
    if (!success(status)) {
        print("LdrLoadDll returned {}\n", status);
        return 1;
    }

    print("{} loaded at {:x}\n", libcurl, (uintptr_t)dll);

    as.Buffer = (char*)funcname.data();
    as.Length = as.MaximumLength = (uint16_t)funcname.size();

    status = LdrGetProcedureAddress(dll, &as, 0, &addr);
    if (!success(status)) {
        print("LdrGetProcedureAddress returned {}\n", status);
        return 1;
    }

    print("{} is at {:x}\n", funcname, (uintptr_t)addr);

    auto func = (func_proto)addr;

    auto ret = func();
    print("{} returned \"{}\"\n", funcname, ret);

    status = LdrUnloadDll(dll);
    if (!success(status)) {
        print("LdrUnloadDll returned {}\n", status);
        return 1;
    }

    return 0;
}
