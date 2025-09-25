module;

#include <stdint.h>
#include <string_view>
#include <span>
#include <string.h>

export module string;

import mem;

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
int tolower(int c) {
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 'a';
    else
        return c;
}

extern "C"
int isprint(int ch) {
    return ch >= ' ' && ch <= '~' ? 1 : 0;
}

extern "C"
int isxdigit(int ch) {
    if (ch >= '0' && ch <= '9')
        return 1;
    else if (ch >= 'a' && ch <= 'f')
        return 1;
    else if (ch >= 'A' && ch <= 'F')
        return 1;
    else
        return 0;
}

extern "C"
int isdigit(int ch) {
    return ch >= '0' && ch <= '9' ? 1 : 0;
}

extern "C"
int isspace(int ch) {
    switch (ch) {
        case ' ':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
        case '\v':
            return 1;

        default:
            return 0;
    }
}

extern "C"
char* strcat(char* dest, const char* src) {
    auto orig_dest = dest;

    while (dest[0] != 0) {
        dest++;
    }

    memcpy(dest, src, strlen(src) + 1);

    return orig_dest;
}

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
size_t strlen(const char* s) {
    size_t i = 0;

    while (s[i] != 0) {
        i++;
    }

    return i;
}

extern "C"
int toupper(int c) {
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';
    else
        return c;
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

extern "C"
char* strstr(const char* haystack, const char* needle) {
    size_t len = strlen(needle);

    while (true) {
        bool found = true;

        for (size_t i = 0; i < len; i++) {
            if (haystack[i] == 0)
                return nullptr;

            if (haystack[i] != needle[i]) {
                found = false;
                break;
            }
        }

        if (found)
            return (char*)haystack;

        haystack++;
    }

    return nullptr;
}
