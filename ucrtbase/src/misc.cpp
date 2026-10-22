// SPDX-License-Identifier: LGPL-2.1-or-later
#include <utility>
#include <algorithm>
#include <stdint.h>
#include <string.h>

import ucrtbase;

#undef errno
import errno;

using namespace std;

extern "C" int* __errno();

static uint64_t rand_seed;
static bool rand_init_done = false;

extern "C"
void srand(unsigned int seed) {
    rand_seed = seed;
}

static void rand_init() {
    if (rand_init_done)
        return;

    rand_init_done = true;

    uint64_t tsc;

#ifdef __x86_64__
    uint32_t lo, hi;

    __asm__ volatile(
        "rdtsc"
        : "=a"(lo), "=d"(hi)
    );

    tsc = ((uint64_t)hi << 32) | lo;
#elifdef __aarch64__
    __asm__ volatile(
        "mrs %0, cntvct_el0"
        : "=r" (tsc)
    );
#else
#error "unhandled architecture"
#endif

    srand((unsigned int)tsc);
}

extern "C"
int rand() {
    rand_init();

    rand_seed = rand_seed * (uint64_t)1103515245 + 12345; // from Knuth

    // like glibc, top bit always clear
    return (int)((rand_seed >> 16) & 0x7fffffff);
}

extern "C"
errno_t rand_s(unsigned int* val) {
    if (!val) {
        *__errno() = EINVAL;
        return EINVAL;
    }

    // FIXME - should be wrapper round getentropy

    *val = (unsigned int)rand();
    return 0;
}

extern "C"
void* bsearch(const void* key, const void* base, size_t nmemb, size_t size,
              int (*compar)(const void*, const void*)) {
    size_t start = 0, end = nmemb;

    while (start < end) {
        size_t mid = start + (end - start) / 2;
        auto ptr = (char*)base + (mid * size);

        auto ret = compar(key, ptr);

        if (ret == 0)
            return ptr;
        else if (ret < 0)
            end = mid;
        else
            start = mid + 1;
    }

    return nullptr;
}

using qsort_func = int (const void* a, const void* b);

extern "C"
void qsort(void* base, size_t nmemb, size_t size, qsort_func compar) {
    if (nmemb <= 1)
        return;

    auto elem = [base, size](ptrdiff_t i) {
        return (uint8_t*)base + ((size_t)i * size);
    };

    auto swap_elems = [&](ptrdiff_t i, ptrdiff_t j) {
        auto p = elem(i);
        auto q = elem(j);
        auto left = size;

        while (left > 0) {
            uint8_t buf[64];
            auto n = min(left, sizeof(buf));

            memcpy(buf, p, n);
            memcpy(p, q, n);
            memcpy(q, buf, n);

            p += n;
            q += n;

            left -= n;
        }
    };

    pair<ptrdiff_t, ptrdiff_t> stack[64];
    size_t top = 0;
    ptrdiff_t low = 0, high = (ptrdiff_t)nmemb - 1;

    while (true) {
        while (low < high) {
            ptrdiff_t m = low + (high - low) / 2;

            if (compar(elem(low), elem(m)) > 0)
                swap_elems(low, m);

            if (compar(elem(low), elem(high)) > 0)
                swap_elems(low, high);

            if (compar(elem(m), elem(high)) > 0)
                swap_elems(m,  high);

            swap_elems(m, high);

            ptrdiff_t store = low;

            for (ptrdiff_t i = low; i < high; i++) {
                if (compar(elem(i), elem(high)) < 0) {
                    swap_elems(store, i);
                    store++;
                }
            }

            swap_elems(store, high);

            ptrdiff_t p = store;

            if (p - low > high - p) {
                if (p - 1 > low) {
                    stack[top] = make_pair(low, p - 1);
                    top++;
                }

                low = p + 1;
            } else {
                if (p + 1 < high) {
                    stack[top] = make_pair(p + 1, high);
                    top++;
                }
                high = p - 1;
            }
        }

        if (top == 0)
            break;

        top--;

        low = stack[top].first;
        high = stack[top].second;
    }
}
