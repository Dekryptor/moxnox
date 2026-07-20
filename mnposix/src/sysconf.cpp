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

        case _SC_ARG_MAX:
            // 65535 bytes maximum for UNICODE_STRING, divided by sizeof(char16_t)
            return 32767;

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

        case _SC_NPROCESSORS_ONLN:
        case _SC_NPROCESSORS_CONF: {
            SYSTEM_BASIC_INFORMATION sbi;

            auto status = NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS::BasicInformation,
                                                   &sbi, sizeof(sbi), nullptr);
            if (!success(status)) {
                *_errno() = nt_status_to_errno(status);
                return -1;
            }

            return sbi.NumberOfProcessors;
        }

        case _SC_PAGESIZE: {
            SYSTEM_BASIC_INFORMATION sbi;

            auto status = NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS::BasicInformation,
                                                   &sbi, sizeof(sbi), nullptr);
            if (!success(status)) {
                *_errno() = nt_status_to_errno(status);
                return -1;
            }

            return sbi.PageSize;
        }

        default:
            *_errno() = EINVAL;
            return -1;
    }
}
