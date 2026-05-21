#include <stdlib.h>
#include <string.h>
#include <string_view>
#include <limits>

import ucrtbase;

using namespace std;

extern "C"
char* _strdup(const char* s) {
    auto sv = string_view(s);

    auto ret = (char*)malloc(sv.size() + 1);
    if (!ret)
        return nullptr;

    memcpy(ret, s, sv.size() + 1);

    return ret;
}

extern "C"
int _stricmp(const char* s1, const char* s2) {
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
int atoi(const char* str) {
    return str_to_num<int>(str);
}
