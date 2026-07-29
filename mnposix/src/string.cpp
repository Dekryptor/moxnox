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
