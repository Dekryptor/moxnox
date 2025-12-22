#include <string_view>
#include <stdint.h>

import nt;
import ntdll;
import shelllib;
import print;

using namespace std;

#ifdef __x86_64__
#define FORCE_ALIGN_ARG_POINTER __attribute__((force_align_arg_pointer))
#else
#define FORCE_ALIGN_ARG_POINTER
#endif

extern "C" FORCE_ALIGN_ARG_POINTER void startup() {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;
    char buf[2];

    buf[0] = 'y';
    buf[1] = '\n';

    while (true) {
        // FIXME - handle short writes

        auto status = NtWriteFile(peb->ProcessParameters->StandardOutput, 0,
                                  nullptr, nullptr, &iosb, buf,
                                  (uint32_t)sizeof(buf), nullptr, nullptr);
        if (!success(status)) {
            // FIXME - GCC ICEs here
            // print("NtWriteFile returned {}\n", status); // FIXME - to stderr
            break;
        }
    }
}
