#include <utility>
#include <stdlib.h>
#include <stdint.h>

import ucrtbase;
import print;

using namespace std;

static int new_mode;

extern "C"
int _set_new_mode(int mode) {
    swap(mode, new_mode);

    return mode;
}

// FIXME - Clang leaks header file from format module?
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-dllimport"

extern "C"
void* _aligned_malloc(size_t size, size_t alignment) {
    auto m = (uint8_t*)malloc(sizeof(uint32_t) + size + alignment - 1);
    if (!m)
        return nullptr;

    auto ret = m;

    if ((uintptr_t)ret % alignment)
        ret += alignment - ((uintptr_t)ret % alignment);

    if ((size_t)(ret - m) < sizeof(uint32_t))
        ret += alignment;

    *(uint32_t*)(ret - sizeof(uint32_t)) = (uint32_t)((size_t)(ret - m) - sizeof(uint32_t));

    return ret;
}

#pragma clang diagnostic pop
