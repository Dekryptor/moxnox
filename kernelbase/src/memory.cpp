// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stddef.h>
#include <stdint.h>

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
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return 0;
    }

    return ret;
}

extern "C"
bool VirtualProtect(void* addr, size_t size, uint32_t new_prot, uint32_t* old_prot) {
    auto status = NtProtectVirtualMemory(NtCurrentProcess(), &addr, &size,
                                         new_prot, old_prot);
    if (!success(status)) {
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return false;
    }

    return true;
}
