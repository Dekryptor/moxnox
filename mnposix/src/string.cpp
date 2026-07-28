#include <string.h>
#include <stdlib.h>

#undef errno
import errno;

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
