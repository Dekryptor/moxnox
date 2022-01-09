#include <stdint.h>

extern "C"
int memcmp(const void* s1, const void* s2, size_t n) {
    // FIXME - not completely accurate, as 1 or -1 is messed up by endianness

    while (n > sizeof(uint64_t)) {
        uint64_t c1 = *(uint64_t*)s1;
        uint64_t c2 = *(uint64_t*)s2;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;

        s1 = (uint64_t*)s1 + 1;
        s2 = (uint64_t*)s2 + 1;
        n -= sizeof(uint64_t);
    }

    while (n > sizeof(uint32_t)) {
        uint32_t c1 = *(uint32_t*)s1;
        uint32_t c2 = *(uint32_t*)s2;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;

        s1 = (uint32_t*)s1 + 1;
        s2 = (uint32_t*)s2 + 1;
        n -= sizeof(uint32_t);
    }

    while (n > sizeof(uint16_t)) {
        uint16_t c1 = *(uint16_t*)s1;
        uint16_t c2 = *(uint16_t*)s2;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;

        s1 = (uint16_t*)s1 + 1;
        s2 = (uint16_t*)s2 + 1;
        n -= sizeof(uint16_t);
    }

    while (n > 0) {
        uint8_t c1 = *(uint8_t*)s1;
        uint8_t c2 = *(uint8_t*)s2;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;

        s1 = (uint8_t*)s1 + 1;
        s2 = (uint8_t*)s2 + 1;
        n--;
    }

    return 0;
}
