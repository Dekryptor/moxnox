#include <stdint.h>

import nt;
import ntdll;
import print;

extern "C"
void RtlSetLastWin32Error(uint32_t err_code) {
    NtCurrentTeb()->LastErrorValue = err_code;
}
