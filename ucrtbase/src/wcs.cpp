#include <stddef.h>
#include <stdint.h>

extern "C"
int __mingw_vswprintf(wchar_t* buf, const wchar_t* format, va_list args);

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

extern "C"
wint_t towupper(wint_t c) {
    // FIXME - other scripts etc.

    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';
    else
        return c;
}

extern "C"
wchar_t* wcsncpy(wchar_t* dest, const wchar_t* src, size_t n) {
    auto orig_dest = dest;

    while (n > 0) {
        if (*src == 0)
            break;

        *dest = *src;
        dest++;
        src++;
        n--;
    }

    while (n > 0) {
        *dest = 0;
        dest++;
        n--;
    }

    return orig_dest;
}

extern "C"
int __stdio_common_vswprintf(uint64_t /*options*/, wchar_t* buffer,
                             size_t /*buffer_count*/, const wchar_t* format,
                             _locale_t /*locale*/, va_list valist) {
    // FIXME - options
    // FIXME - locale
    // FIXME - buffer_count

    return __mingw_vswprintf(buffer, format, valist);
}

extern "C"
int iswspace(wint_t ch) {
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
