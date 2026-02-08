#include <string_view>
#include <stdint.h>

import nt;
import ntdll;
import print;

using namespace std;

extern "C" void startup() {
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
            print_stderr("NtWriteFile returned {}\n", status);
            break;
        }
    }
}
