#include <string_view>
#include <stdint.h>

import nt;
import ntdll;
import print;

using namespace std;

int main() {
    UNICODE_STRING us;
    void* dll;

    static const u16string_view libcurl = u"libcurl.dll";

    us.Buffer = (char16_t*)libcurl.data();
    us.Length = us.MaximumLength = (uint16_t)(libcurl.size() * sizeof(char16_t));

    auto status = LdrLoadDll(nullptr, nullptr, &us, &dll);
    if (!success(status)) {
        print("LdrLoadDll returned {}\n", status);
        return 1;
    }

    print("{} loaded at {:x}\n", libcurl, (uintptr_t)dll);

    // FIXME - LdrGetProcedureAddress
    // FIXME - LdrUnloadDll

    return 0;
}
