#include <stdint.h>

import ntdll;
import print;

extern "C"
void RtlSetLastWin32Error(uint32_t err_code) {
    print_stderr("FIXME - RtlSetLastWin32Error {}\n", err_code); // FIXME
}
