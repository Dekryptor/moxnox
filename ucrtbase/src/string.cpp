#define _CRTBLD
#include <stdlib.h>

#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <string_view>
#include <limits>
#include <charconv>

import ucrtbase;

using namespace std;

extern "C" int* __errno();

extern "C"
char* __strdup(const char* s) {
    auto sv = string_view(s);

    auto ret = (char*)malloc(sv.size() + 1);
    if (!ret)
        return nullptr;

    memcpy(ret, s, sv.size() + 1);

    return ret;
}

extern "C"
int __stricmp(const char* s1, const char* s2) {
    size_t i = 0;

    // FIXME - Unicode?

    while (true) {
        char c1 = s1[i];
        char c2 = s2[i];

        if (c1 >= 'A' && c1 <= 'Z')
            c1 = c1 - 'A' + 'a';

        if (c2 >= 'A' && c2 <= 'Z')
            c2 = c2 - 'A' + 'a';

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

extern "C"
int __strnicmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        auto c1 = *s1;
        auto c2 = *s2;

        if (c1 >= 'A' && c1 <= 'Z')
            c1 = c1 - 'A' + 'a';

        if (c2 >= 'A' && c2 <= 'Z')
            c2 = c2 - 'A' + 'a';

        if (c1 < c2)
            return -1;
        else if (c2 < c1)
            return 1;

        if (c1 == 0)
            break;

        s1++;
        s2++;
    }

    return 0;
}

extern "C"
size_t strcspn(const char* s, const char* reject) {
    auto rl = strlen(reject);
    size_t ret = 0;

    while (*s) {
        for (size_t i = 0; i < rl; i++) {
            if (*s == reject[i])
                return ret;
        }

        s++;
        ret++;
    }

    return ret;
}

extern "C"
size_t strspn(const char* s, const char* accept) {
    auto al = strlen(accept);
    size_t ret = 0;

    while (*s) {
        bool match = false;
        for (size_t i = 0; i < al; i++) {
            if (*s == accept[i]) {
                match = true;
                break;
            }
        }

        if (!match)
            return ret;

        s++;
        ret++;
    }

    return ret;
}

extern "C"
char* strpbrk(const char* s, const char* accept) {
    auto al = strlen(accept);

    while (*s) {
        for (size_t i = 0; i < al; i++) {
            if (*s == accept[i])
                return (char*)s;
        }

        s++;
    }

    return nullptr;
}

template<signed_integral T>
static T str_to_num(const char* str) {
    bool negative = false;

    if (!str)
        return 0;

    while (str[0] == ' ' || str[0] == '\t') {
        str++;
    }

    if (str[0] == '-') {
        negative = true;
        str++;
    } else if (str[0] == '+')
        str++;

    T ret = 0;

    while (str[0] >= '0' && str[0] <= '9') {
        int digit = str[0] - '0';

        if (!negative && (ret > numeric_limits<T>::max() / 10 || (ret == numeric_limits<T>::max() / 10 && digit > numeric_limits<T>::max() % 10)))
            return numeric_limits<T>::max();
        else if (negative && (-ret < numeric_limits<T>::min() / 10 || (-ret == numeric_limits<T>::min() / 10 && -digit < numeric_limits<T>::min() % 10)))
            return numeric_limits<T>::min();

        ret *= 10;
        ret += digit;

        str++;
    }

    if (negative)
        return -ret;
    else
        return ret;
}

extern "C"
long atol(const char* str) {
    return str_to_num<long>(str);
}

extern "C"
long long atoll(const char* str) {
    return str_to_num<long long>(str);
}

extern "C"
int atoi(const char* str) {
    return str_to_num<int>(str);
}

extern "C" double __mingw_strtod(const char* nptr, char** endptr);
extern "C" float __mingw_strtof(const char* nptr, char** endptr);

extern "C"
double strtod(const char* nptr, char** endptr) {
    return __mingw_strtod(nptr, endptr);
}

extern "C"
float strtof(const char* nptr, char** endptr) {
    return __mingw_strtof(nptr, endptr);
}

extern "C"
double atof(const char* str) {
    return __mingw_strtod(str, nullptr);
}

extern "C"
int _strcoll(const char* s1, const char* s2) {
    // FIXME - potentially different if locale not default

    return strcmp(s1, s2);
}

extern "C"
int wcscoll(const wchar_t* s1, const wchar_t* s2) {
    // FIXME - potentially different if locale not default

    return wcscmp(s1, s2);
}

template<integral T>
static T strto(const char* nptr, char** endptr, int base) {
    auto orig_nptr = nptr;

    while (isspace(*nptr)) {
        nptr++;
    }

    bool neg = false;

    if (*nptr == '+') {
        nptr++;

        if (*nptr == '-') {
            if (endptr)
                *endptr = (char*)orig_nptr;

            *__errno() = EINVAL;
            return 0;
        }
    } else if (*nptr == '-') {
        neg = true;
        nptr++;
    }

    T ret;

    if (base == 0) {
        if (nptr[0] == '0' && (nptr[1] == 'x' || nptr[1] == 'X') && isxdigit(nptr[2])) {
            base = 16;
            nptr += 2;
        } else if (nptr[0] == '0')
            base = 8;
        else
            base = 10;
    } else if (base == 16) {
        if (nptr[0] == '0' && (nptr[1] == 'x' || nptr[1] == 'X') && isxdigit(nptr[2]))
            nptr += 2;
    }

    auto fcr = from_chars(nptr, nptr + strlen(nptr), ret, base);

    if (fcr.ec == errc::invalid_argument) {
        if (endptr)
            *endptr = (char*)orig_nptr;

        return 0;
    }

    if (endptr)
        *endptr = (char*)fcr.ptr;

    if (fcr.ec == errc::result_out_of_range) {
        *__errno() = ERANGE;

        if constexpr (is_signed_v<T>) {
            if (neg)
                return numeric_limits<T>::min();
        }

        return numeric_limits<T>::max();
    }

    if (neg)
        ret = -ret;

    return ret;
}

extern "C"
long long strtoll(const char* nptr, char** endptr, int base) {
    return strto<long long>(nptr, endptr, base);
}

extern "C"
unsigned long long strtoull(const char* nptr, char** endptr, int base) {
    return strto<unsigned long long>(nptr, endptr, base);
}

extern "C"
long strtol(const char* nptr, char** endptr, int base) {
    return strto<long>(nptr, endptr, base);
}

extern "C"
unsigned long strtoul(const char* nptr, char** endptr, int base) {
    return strto<unsigned long>(nptr, endptr, base);
}

extern "C"
intmax_t strtoimax(const char* nptr, char** endptr, int base) {
    return strto<intmax_t>(nptr, endptr, base);
}

extern "C"
uintmax_t strtoumax(const char* nptr, char** endptr, int base) {
    return strto<uintmax_t>(nptr, endptr, base);
}

extern "C"
char* _strtok(char* str, const char* delim) {
    static char* s;

    if (str)
        s = str;

    auto delimlen = strlen(delim);

    // skip leading delimiters

    while (true) {
        if (s[0] == 0)
            return nullptr;

        bool found = false;
        for (size_t i = 0; i < delimlen; i++) {
            if (s[0] == delim[i]) {
                found = true;
                break;
            }
        }

        if (!found)
            break;

        s++;
    }

    auto ret = s;

    while (true) {
        if (s[0] == 0)
            return ret;

        for (size_t i = 0; i < delimlen; i++) {
            if (s[0] == delim[i]) {
                s[0] = 0;
                s++;

                return ret;
            }
        }

        s++;
    }
}

extern "C"
char* strncat(char* dest, const char* src, size_t n) {
    size_t len;
    auto orig_dest = dest;

    while (dest[0] != 0) {
        dest++;
    }

    len = 0;
    for (size_t i = 0; i < n; i++) {
        if (src[i] == 0)
            break;

        len++;
    }

    memcpy(dest, src, len);
    dest[len] = 0;

    return orig_dest;
}

extern "C"
size_t strxfrm(char* dest, const char* src, size_t size) {
    auto len = strlen(src);

    if (size > 0)
        memcpy(dest, src, min(len + 1, size));

    return len;
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
