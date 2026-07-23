#include <string.h>

import ucrtbase;

extern "C"
int strerror_r(int errnum, char* strerrbuf, size_t buflen) {
    auto ret = strerror_s(strerrbuf, buflen, errnum);

    if (ret) {
        *_errno() = ret;
        return -1;
    }

    return 0;
}
