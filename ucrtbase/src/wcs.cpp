#include <stddef.h>

extern "C"
size_t wcsnlen(const wchar_t* s, size_t maxlen) {
    size_t i = 0;

    while (i < maxlen && s[i] != 0) {
        i++;
    }

    return i;
}

extern "C"
wint_t towlower(wint_t c) {
    // FIXME - other scripts etc.

    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 'a';
    else
        return c;
}
