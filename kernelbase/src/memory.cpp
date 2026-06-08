#include <stddef.h>

import nt;
import kernelbase;
import print;

extern "C"
size_t VirtualQuery(const void* addr, MEMORY_BASIC_INFORMATION* /*info*/, size_t len) {
    print("FIXME - VirtualQuery {:x}, {:x}\n", (uintptr_t)addr, len);

    // FIXME

    return 0;
}
