#include <ntdll.h>
#include <string_view>
#include <span>

using namespace std;

import shelllib;
import unicode;

extern "C"
void* malloc(size_t size) {
    return RtlAllocateHeap(GetProcessHeap(), 0, size);
}

extern "C"
void free(void* ptr) {
    RtlFreeHeap(GetProcessHeap(), 0, ptr);
}

extern "C"
void startup() {
    const auto& teb = *NtCurrentTeb();
    const auto& peb = *teb.ProcessEnvironmentBlock;
    const auto& params = *peb.ProcessParameters;

    auto pwd = u16string_view(params.CurrentDirectory.DosPath.Buffer,
                              params.CurrentDirectory.DosPath.Length / sizeof(char16_t));
    auto len = utf16_to_utf8_len(pwd);

    if (len != 0) {
        auto buf = (char*)malloc(len);
        if (!buf) {
            print("malloc failed\n");
            NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
            return;
        }

        utf16_to_utf8(span(buf, len), pwd);

        print(string_view(buf, len));
        free(buf);
    }

    print("\n");

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
