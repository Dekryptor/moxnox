// SPDX-License-Identifier: LGPL-2.1-or-later
#include <sched.h>

import nt;
import ucrtbase;

#undef errno
import errno;

extern "C"
int sched_yield() {
    auto status = NtYieldExecution();
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int sched_get_priority_min(int policy) {
    if (policy == SCHED_OTHER)
        return 0;
    else {
        *_errno() = EINVAL;
        return -1;
    }
}

extern "C"
int sched_get_priority_max(int policy) {
    if (policy == SCHED_OTHER)
        return 0;
    else {
        *_errno() = EINVAL;
        return -1;
    }
}
