#include <sys/resource.h>
#include <errno.h>
#include <stdint.h>

import print;
import nt;
import pe;

#undef errno
import errno;

extern "C"
int getrlimit(int resource, struct rlimit* rlp) {
    switch (resource) {
        case RLIMIT_NOFILE:
            // from ucrtbase's fd table
            rlp->rlim_cur = 2048;
            rlp->rlim_max = 2048;
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
