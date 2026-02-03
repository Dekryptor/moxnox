module;

#include <stdint.h>
#include <string.h>
#include <charconv>

export module crt;

using namespace std;

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

export
extern "C"
int memcmp(const void* s1, const void* s2, size_t n) {
    // FIXME - not completely accurate, as 1 or -1 is messed up by endianness

    while (n > sizeof(uint64_t)) {
        uint64_t c1 = *(__unaligned uint64_t*)s1;
        uint64_t c2 = *(__unaligned uint64_t*)s2;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;

        s1 = (uint64_t*)s1 + 1;
        s2 = (uint64_t*)s2 + 1;
        n -= sizeof(uint64_t);
    }

    while (n > sizeof(uint32_t)) {
        uint32_t c1 = *(__unaligned uint32_t*)s1;
        uint32_t c2 = *(__unaligned uint32_t*)s2;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;

        s1 = (uint32_t*)s1 + 1;
        s2 = (uint32_t*)s2 + 1;
        n -= sizeof(uint32_t);
    }

    while (n > sizeof(uint16_t)) {
        uint16_t c1 = *(__unaligned uint16_t*)s1;
        uint16_t c2 = *(__unaligned uint16_t*)s2;

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

export
extern "C"
void* memmove(void* dest, const void* src, size_t n) {
    auto orig_dest = dest;

    if (dest <= src) {
        while (n > 0) {
            *(uint8_t*)dest = *(uint8_t*)src;

            dest = (uint8_t*)dest + 1;
            src = (uint8_t*)src + 1;

            n--;
        }
    } else {
        dest = (uint8_t*)dest + n - 1;
        src = (uint8_t*)src + n - 1;

        while (n > 0) {
            *(uint8_t*)dest = *(uint8_t*)src;

            dest = (uint8_t*)dest - 1;
            src = (uint8_t*)src - 1;

            n--;
        }
    }

    return orig_dest;
}

export
extern "C"
size_t strlen(const char* s) {
    size_t i = 0;

    while (s[i] != 0) {
        i++;
    }

    return i;
}

export
extern "C"
void* memchr(const void* ptr, int ch, size_t count) {
    auto p = (uint8_t*)ptr;

    for (size_t i = 0; i < count; i++) {
        if (*p == ch)
            return p;

        p++;
    }

    return nullptr;
}

export
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

export
extern "C"
int tolower(int c) {
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 'a';
    else
        return c;
}

export
extern "C"
int toupper(int c) {
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';
    else
        return c;
}

export
extern "C"
int isprint(int ch) {
    return ch >= ' ' && ch <= '~' ? 1 : 0;
}

export
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

export
extern "C"
int isdigit(int ch) {
    return ch >= '0' && ch <= '9' ? 1 : 0;
}

export
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

export
extern "C"
char* strcat(char* dest, const char* src) {
    auto orig_dest = dest;

    while (dest[0] != 0) {
        dest++;
    }

    memcpy(dest, src, strlen(src) + 1);

    return orig_dest;
}

export
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

export
extern "C"
char* strcpy(char* dest, const char* src) {
    char* orig_dest = dest;

    while (*src != 0) {
        *dest = *src;
        src++;
        dest++;
    }

    *dest = 0;

    return orig_dest;
}

export
extern "C"
int strcmp(const char* s1, const char* s2) {
    size_t i = 0;

    while (true) {
        char c1 = s1[i];
        char c2 = s2[i];

        if (c1 == 0 && c2 == 0)
            return 0;
        else if (c1 == 0)
            return -1;
        else if (c2 == 0)
            return 1;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;

        i++;
    }
}

export
extern "C"
int strncmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c1 = s1[i];
        char c2 = s2[i];

        if (c1 == 0 && c2 == 0)
            return 0;
        else if (c1 == 0)
            return -1;
        else if (c2 == 0)
            return 1;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;
    }

    return 0;
}

export
extern "C"
char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == c)
            return (char*)s;

        s++;
    }

    return nullptr;
}

template<typename T>
static __attribute__((always_inline)) T inline strto(const char* nptr, char** endptr, int base) {
    while (*nptr == ' ' || *nptr == '\t') {
        nptr++;
    }

    unsigned long ret;

    auto fcr = from_chars(nptr, nptr + strlen(nptr), ret, base);

    if (endptr)
        *endptr = (char*)fcr.ptr;

    return (T)ret;
}

export
extern "C"
long strtol(const char* nptr, char** endptr, int base) {
    return strto<long>(nptr, endptr, base);
}

export
extern "C"
unsigned long strtoul(const char* nptr, char** endptr, int base) {
    return strto<unsigned long>(nptr, endptr, base);
}

#ifdef __x86_64__
export extern "C" void* memcpy(void* dest, const void* src, size_t n);
#else
export
extern "C"
void* memcpy(void* dest, const void* src, size_t n) {
    void* orig_dest = dest;

    if (!((uintptr_t)dest & (sizeof(uint64_t) - 1)) && !((uintptr_t)src & (sizeof(uint64_t) - 1))) {
        while (n >= sizeof(uint64_t)) {
            *(uint64_t*)dest = *(uint64_t*)src;

            dest = (uint8_t*)dest + sizeof(uint64_t);
            src = (uint8_t*)src + sizeof(uint64_t);

            n -= sizeof(uint64_t);
        }
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

export
extern "C"
size_t wcslen(const wchar_t* s) {
    size_t i = 0;

    while (s[i] != 0) {
        i++;
    }

    return i;
}

export
extern "C"
int wcscmp(const wchar_t* s1, const wchar_t* s2) {
    size_t i = 0;

    while (true) {
        wchar_t c1 = s1[i];
        wchar_t c2 = s2[i];

        if (c1 == 0 && c2 == 0)
            return 0;
        else if (c1 == 0)
            return -1;
        else if (c2 == 0)
            return 1;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;

        i++;
    }
}

export
extern "C"
char* strrchr(const char* s, int c) {
    char* last = nullptr;

    while (*s) {
        if (*s == c)
            last = (char*)s;

        s++;
    }

    return last;
}
