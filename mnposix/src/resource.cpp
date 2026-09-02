#include <sys/resource.h>
#include <errno.h>
#include <stdint.h>

import print;
import nt;
import pe;

#undef errno
import errno;

// from ucrtbase's fd table
static constexpr size_t MAX_FDS_NUM = 2048;

extern "C"
int getrlimit(int resource, struct rlimit* rlp) {
    switch (resource) {
        case RLIMIT_NOFILE:
            rlp->rlim_cur = MAX_FDS_NUM;
            rlp->rlim_max = MAX_FDS_NUM;
            break;

        case RLIMIT_DATA:
        case RLIMIT_CPU:
        case RLIMIT_FSIZE:
        case RLIMIT_AS:
            // unenforced
            rlp->rlim_cur = RLIM_INFINITY;
            rlp->rlim_max = RLIM_INFINITY;
            break;

        case RLIMIT_CORE:
            // no core dumps
            rlp->rlim_cur = 0;
            rlp->rlim_max = 0;
            break;

        case RLIMIT_STACK: {
            auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
            auto& dos_header = *(IMAGE_DOS_HEADER*)peb.ImageBaseAddress;
            auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)&dos_header + dos_header.e_lfanew);

            if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
                rlp->rlim_cur = (rlim_t)nt_header.OptionalHeader64.SizeOfStackReserve;
            else
                rlp->rlim_cur = (rlim_t)nt_header.OptionalHeader32.SizeOfStackReserve;

            rlp->rlim_max = rlp->rlim_cur;
            break;
        }

        default:
            *_errno() = EINVAL;
            return -1;
    }

    return 0;
}

extern "C"
int setrlimit(int resource, const rlimit* rlp) {
    if (rlp->rlim_cur > rlp->rlim_max) {
        *_errno() = EINVAL;
        return -1;
    }

    switch (resource) {
        case RLIMIT_NOFILE:
            if (rlp->rlim_max > MAX_FDS_NUM) {
                *_errno() = EPERM;
                return -1;
            }
            break;

        case RLIMIT_DATA:
        case RLIMIT_CPU:
        case RLIMIT_FSIZE:
        case RLIMIT_AS:
            // ignore
            break;

        case RLIMIT_CORE:
            if (rlp->rlim_max > 0) {
                *_errno() = EPERM;
                return -1;
            }
            break;

        case RLIMIT_STACK: {
            rlimit r;

            if (getrlimit(resource, &r))
                return -1;

            if (rlp->rlim_max > r.rlim_max) {
                *_errno() = EPERM;
                return -1;
            }

            break;
        }

        default:
            *_errno() = EINVAL;
            return -1;
    }

    return 0;
}

extern "C"
int getpriority(int which, id_t /*who*/) {
    if (which != PRIO_PROCESS && which != PRIO_PGRP && which != PRIO_USER) {
        *_errno() = EINVAL;
        return -1;
    }

    // FIXME

    return 0;
}
