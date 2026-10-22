// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

static inline int a_clz_64(uint64_t x) {
    return __builtin_clzll(x);
}
