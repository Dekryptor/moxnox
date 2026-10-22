// SPDX-License-Identifier: LGPL-2.1-or-later
#include <utility>
#include <algorithm>
#include <stdlib.h>
#include <stdint.h>

import ucrtbase;

using namespace std;

static int new_mode;

extern "C"
int _set_new_mode(int mode) {
    swap(mode, new_mode);

    return mode;
}

extern "C"
void* __aligned_malloc(size_t size, size_t alignment) {
    alignment = max(alignment, sizeof(uint32_t));

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

extern "C"
void __aligned_free(void* ptr) {
    auto& n = *(uint32_t*)((uint8_t*)ptr - sizeof(uint32_t));

    auto m = (uint8_t*)ptr - sizeof(uint32_t) - n;

    free(m);
}
