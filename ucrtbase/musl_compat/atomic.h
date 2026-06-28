#pragma once

static inline int a_clz_64(uint64_t x) {
    return __builtin_clzll(x);
}
