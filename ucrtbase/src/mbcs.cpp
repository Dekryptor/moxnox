#include <stddef.h>

import ucrtbase;
import print;

extern "C"
size_t _mbrtowc(wchar_t* pwc, const char* s, size_t n, mbstate_t* ps) {
    print("FIXME - mbrtowc\n"); // FIXME

    return 0;
}
