#include <string_view>
#include <span>
#include <stdlib.h>

using namespace std;

import nt;
import ntdll;
import shelllib;
import unicode;

#ifdef __x86_64__
#define FORCE_ALIGN_ARG_POINTER __attribute__((force_align_arg_pointer))
#else
#define FORCE_ALIGN_ARG_POINTER
#endif

extern "C" FORCE_ALIGN_ARG_POINTER void startup() {
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
