#include <stddef.h>
#include <stdint.h>

import ucrtbase;
import print;

extern "C"
size_t _mbrtowc(wchar_t* pwc, const char* s, size_t n, mbstate_t* /*ps*/) {
    if (!s)
        return 0;

    if (n == 0)
        return (size_t)-2;

    auto c = (uint8_t)s[0];

    if (c < 0x80) {
        if (pwc)
            *pwc = c;

        return c == 0 ? 0 : 1;
    }

    // FIXME - proper UTF-8 handling

    *_errno() = EILSEQ;
    return (size_t)-1;
}

extern "C"
size_t _wcrtomb(char* /*s*/, wchar_t /*wc*/, mbstate_t* /*ps*/) {
    print("FIXME - wcrtomb\n");

    return 0;
}
