#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <algorithm>

import ucrtbase;

#undef errno
import errno;

using namespace std;

extern "C"
char* strndup(const char* s, size_t n) {
    auto len = strnlen(s, n);

    auto ret = (char*)malloc(len + 1);
    if (!ret) {
        *_errno() = ENOMEM;
        return nullptr;
    }

    memcpy(ret, s, len);
    ret[len] = 0;

    return ret;
}

extern "C"
size_t mbsnrtowcs(wchar_t* pwcs, const char** s, size_t nms, size_t n,
                  mbstate_t* state) {
    static mbstate_t static_state;
    size_t count = 0;
    auto st = state ? state : &static_state;
    auto p = *s;
    size_t remaining = nms;

    while ((!pwcs || count < n) && remaining > 0) {
        wchar_t wc;

        auto res = mbrtowc(pwcs ? &wc : nullptr, p, min(remaining, (size_t)MB_CUR_MAX),
                           st);

        if (res == 0) {
            if (pwcs) {
                pwcs[count] = L'\0';
                *s = nullptr;
            }

            return count;
        }

        if (res == (size_t)-1) {
            if (pwcs)
                *s = p;

            return (size_t)-1;
        }

        if (res == (size_t)-2) {
            p += remaining;
            break;
        }

        if (pwcs)
            pwcs[count] = wc;

        p += res;
        remaining -= res;

        count++;
    }

    if (pwcs)
        *s = p;

    return count;
}

extern "C"
size_t wcsnrtombs(char* dest, const wchar_t** src, size_t nwc, size_t dsize,
                  mbstate_t* ps) {
    static mbstate_t static_state;
    size_t total = 0;
    auto st = ps ? ps : &static_state;
    auto p = *src;
    size_t remaining = nwc;

    while (remaining > 0) {
        char buf[4];

        size_t n = wcrtomb(buf, *p, st);

        if (n == (size_t)-1) {
            if (dest)
                *src = p;

            return (size_t)-1;
        }

        if (dest && total + n > dsize) {
            *src = p;
            return total;
        }

        if (*p == 0) {
            if (dest) {
                *dest = 0;
                *src = nullptr;
            }

            return total;
        }

        if (dest) {
            memcpy(dest, buf, n);
            dest += n;
        }

        total += n;
        remaining--;
        p++;
    }

    if (dest)
        *src = p;

    return total;
}
