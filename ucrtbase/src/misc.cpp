#include <stdint.h>

import ucrtbase;

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
void* bsearch(const void* key, const void* base, size_t nmemb, size_t size,
              int (*compar)(const void*, const void*)) {
    size_t start = 0, end = nmemb - 1;

    if (nmemb == 0)
        return nullptr;

    while (start <= end) {
        size_t mid = (start + end) / 2;
        auto ptr = (char*)base + (mid * size);

        auto ret = compar(key, ptr);

        if (ret == 0)
            return ptr;
        else if (ret < 0)
            end = mid - 1;
        else
            start = mid + 1;
    }

    return nullptr;
}
