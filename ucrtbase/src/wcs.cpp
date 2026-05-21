#include <stddef.h>

extern "C"
size_t wcsnlen(const wchar_t* s, size_t maxlen) {
    size_t i = 0;

    while (i < maxlen && s[i] != 0) {
        i++;
    }

    return i;
}
