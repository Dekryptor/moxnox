#include <wchar.h>

extern "C"
int wcwidth(wchar_t /*wc*/) {
    // FIXME

    return 1;
}

extern "C"
int wcswidth(const wchar_t* pwcs, size_t n) {
    int ret = 0;

    // FIXME - surrogate pairs

    for (size_t i = 0; i < n; i++) {
        if (pwcs[i] == 0)
            break;

        auto w = wcwidth(pwcs[i]);
        if (w == -1)
            return -1;

        ret += w;
    }

    return ret;
}
