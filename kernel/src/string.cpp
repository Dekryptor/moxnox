module;

#include <stdint.h>
#include <string_view>
#include <span>

export module string;

import mem;
import crt;

using namespace std;

#ifdef __aarch64__
extern "C"
void* memcpy(void* dest, const void* src, size_t n) {
    void* orig_dest = dest;

#if __INTPTR_WIDTH__ == 64
    while (n >= sizeof(uint64_t)) {
        *(__unaligned uint64_t*)dest = *(__unaligned uint64_t*)src;

        dest = (uint8_t*)dest + sizeof(uint64_t);
        src = (uint8_t*)src + sizeof(uint64_t);

        n -= sizeof(uint64_t);
    }
#endif

    while (n >= sizeof(uint32_t)) {
        *(__unaligned uint32_t*)dest = *(__unaligned uint32_t*)src;

        dest = (uint8_t*)dest + sizeof(uint32_t);
        src = (uint8_t*)src + sizeof(uint32_t);

        n -= sizeof(uint32_t);
    }

    while (n >= sizeof(uint16_t)) {
        *(__unaligned uint16_t*)dest = *(__unaligned uint16_t*)src;

        dest = (uint8_t*)dest + sizeof(uint16_t);
        src = (uint8_t*)src + sizeof(uint16_t);

        n -= sizeof(uint16_t);
    }

    while (n >= sizeof(uint8_t)) {
        *(uint8_t*)dest = *(uint8_t*)src;

        dest = (uint8_t*)dest + sizeof(uint8_t);
        src = (uint8_t*)src + sizeof(uint8_t);

        n -= sizeof(uint8_t);
    }

    return orig_dest;
}
#endif

extern "C"
char* strncpy(char* dst, const char* src, size_t dsize) {
    char* orig_dst = dst;

    while (dsize > 0) {
        if (*src == 0)
            break;

        *dst = *src;
        dst++;
        src++;
        dsize--;
    }

    while (dsize > 0) {
        *dst = 0;
        dst++;
        dsize--;
    }

    return orig_dst;
}

extern "C"
long int strtol(const char* nptr, char** endptr, int base) {
    long int val;

    while (*nptr == ' ' || *nptr == '\t') {
        nptr++;
    }

    val = 0;

    while (true) {
        if (*nptr >= '0' && *nptr <= '9') {
            val *= base;
            val += *nptr - '0';
        } else {
            if (endptr)
                *endptr = (char*)nptr;

            return val;
        }

        nptr++;
    }
}
