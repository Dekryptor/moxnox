module;

#include <stdint.h>

export module crt;

export
extern "C"
void* memset(void* s, int c, size_t n) {
    void* orig_s = s;

    // FIXME - do SSE version of this

    while ((uintptr_t)s & (sizeof(uint64_t) - 1) && n > 0) {
        *(uint8_t*)s = (uint8_t)c;

        s = (uint8_t*)s + 1;
        n--;
    }

    if (n == 0)
        return orig_s;

    uint64_t v;

    v = 0;

    for (unsigned int i = 0; i < sizeof(uint64_t); i++) {
        v <<= 8;
        v |= c & 0xff;
    }

    while (n >= sizeof(uint64_t)) {
        *(uint64_t*)s = v;

        s = (uint8_t*)s + sizeof(uint64_t);
        n -= sizeof(uint64_t);
    }

    while (n > 0) {
        *(uint8_t*)s = (uint8_t)c;

        s = (uint8_t*)s + 1;
        n--;
    }

    return orig_s;
}
