module;

#include <stdint.h>
#include <string_view>
#include <span>
#include <string.h>

export module string;

import mem;

using namespace std;

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
