#include "../../modules/lconv.h"

import errno;
import ucrtbase;
import print;

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" int* __errno();

struct mbstate {
    uint16_t codepoint;
    uint8_t left;
    uint8_t total;
};

static_assert(sizeof(mbstate) <= sizeof(int));

extern "C"
size_t _mbrtowc(wchar_t* pwc, const char* s, size_t n, mbstate_t* ps) {
    static mbstate_t static_state;

    auto& st = *reinterpret_cast<mbstate*>(ps ? ps : &static_state);

    if (!s) {
        if (st.left > 0) {
            st.left = 0;
            *__errno() = EILSEQ;
            return (size_t)-1;
        }

        return 0;
    }

    if (n == 0)
        return (size_t)-2;

    size_t read = 0;
    uint16_t cp;
    uint8_t left, total;

    if (st.left > 0) {
        cp = st.codepoint;
        left = st.left;
        total = st.total;
    } else {
        auto c = (uint8_t)s[read];
        read++;

        if (c < 0x80) {
            if (pwc)
                *pwc = c;

            return c == 0 ? 0 : 1;
        }

        if (c < 0xc2) {
            *__errno() = EILSEQ;
            return (size_t)-1;
        } else if (c < 0xe0) {
            cp = c & 0x1f;
            left = 1;
            total = 1;
        } else if (c < 0xf0) {
            cp = c & 0x0f;
            left = 2;
            total = 2;
        } else {
            *__errno() = EILSEQ;
            return (size_t)-1;
        }
    }

    while (left > 0) {
        if (read >= n) {
            st.codepoint = cp;
            st.left = left;
            st.total = total;

            return (size_t)-2;
        }

        auto c = (uint8_t)s[read];
        read++;

        if (c < 0x80 || c > 0xbf) {
            st.left = 0;
            *__errno() = EILSEQ;
            return (size_t)-1;
        }

        cp = (uint16_t)((cp << 6) | (c & 0x3f));
        left--;
    }

    if ((total == 2 && cp < 0x800) || (cp >= 0xd800 && cp <= 0xdfff)) {
        st.left = 0;
        *__errno() = EILSEQ;
        return (size_t)-1;
    }

    st.left = 0;

    if (pwc)
        *pwc = (wchar_t)cp;

    return read;
}

extern "C"
size_t _wcrtomb(char* s, wchar_t wc, mbstate_t* /*ps*/) {
    if (!s)
        return 1;

    if (wc < 0x80) {
        *s = (char)wc;
        return 1;
    }

    if (wc < 0x800) {
        s[0] = (char)(0xc0 | (wc >> 6));
        s[1] = (char)(0x80 | (wc & 0x3f));

        return 2;
    }

    // FIXME?
    if (wc >= 0xd800 && wc <= 0xdfff) {
        *__errno() = EILSEQ;
        return (size_t)-1;
    }

    s[0] = (char)(0xe0 | (wc >> 12));
    s[1] = (char)(0x80 | ((wc >> 6) & 0x3f));
    s[2] = (char)(0x80 | (wc & 0x3f));

    return 3;
}

extern "C"
size_t _mbrlen(const char* s, size_t n, mbstate_t* ps) {
    static mbstate_t state;

    return _mbrtowc(nullptr, s, n, ps ? ps : &state);
}

extern "C"
int ____mb_cur_max_func() {
    // maximum bytes per code-point for UTF-8

    return 4;
}

extern "C"
size_t _wcsrtombs(char* dest, const wchar_t** src, size_t dsize, mbstate_t* ps) {
    size_t total = 0;

    while (true) {
        char buf[4];

        size_t n = _wcrtomb(buf, **src, ps);

        if (n == (size_t)-1)
            return (size_t)-1;

        if (**src == 0) {
            if (dest) {
                *dest = 0;
                *src = nullptr;
            }

            return total;
        }

        if (dest) {
            if (n > dsize)
                return total;

            memcpy(dest, buf, n);
            dest += n;
            dsize -= n;
        }

        total += n;
        (*src)++;
    }
}
