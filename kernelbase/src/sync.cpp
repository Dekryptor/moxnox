#include <stdint.h>

import nt;

extern "C" __declspec(dllexport)
void Sleep(uint32_t timeout) {
    LARGE_INTEGER t;

    // convert from milliseconds to units of 100ns
    t.QuadPart = -(int64_t)timeout * 10000;

    NtDelayExecution(false, &t);
}
