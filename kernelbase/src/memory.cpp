#include <stddef.h>

import nt;
import ntdll;
import kernelbase;

extern "C"
size_t VirtualQuery(const void* addr, MEMORY_BASIC_INFORMATION* info, size_t len) {
    size_t ret;

    auto status = NtQueryVirtualMemory(NtCurrentProcess(), addr,
                                       MEMORY_INFORMATION_CLASS::BasicInformation,
                                       info, len, &ret);
    if (!success(status)) {
        SetLastError(RtlNtStatusToDosError(status));
        return 0;
    }

    return ret;
}
