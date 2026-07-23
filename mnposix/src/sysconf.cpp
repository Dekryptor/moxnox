#include <unistd.h>
#include <errno.h>

import nt;

#undef errno
import errno;

extern "C"
long sysconf(int name) {
    switch (name) {
        case _SC_CLK_TCK:
            return 1000;

        case _SC_PHYS_PAGES: {
            SYSTEM_BASIC_INFORMATION sbi;

            auto status = NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS::BasicInformation,
                                                   &sbi, sizeof(sbi), nullptr);
            if (!success(status)) {
                *_errno() = nt_status_to_errno(status);
                return -1;
            }

            return sbi.NumberOfPhysicalPages;
        }

        default:
            *_errno() = EINVAL;
            return -1;
    }
}
