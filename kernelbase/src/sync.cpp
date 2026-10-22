// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stdint.h>

import nt;

extern "C"
void Sleep(uint32_t timeout) {
    LARGE_INTEGER t;

    // convert from milliseconds to units of 100ns
    t.QuadPart = -(int64_t)timeout * 10000;

    NtDelayExecution(false, &t);
}

extern "C"
uint32_t SleepEx(uint32_t timeout, bool alertable) {
    nt_status status;
    LARGE_INTEGER t;

    // convert from milliseconds to units of 100ns
    t.QuadPart = -(int64_t)timeout * 10000;

    status = NtDelayExecution(alertable, &t);

    if (status == nt_status::USER_APC)
        return (uint32_t)status;

    return 0;
}
