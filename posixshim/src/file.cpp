#include <sys/types.h>
#include <sys/stat.h>

import ucrtbase;
import print;

#undef errno
import errno;

extern "C"
int mkdir(const char* path, mode_t mode) {
    print("FIXME - mkdir({}, {})\n", path, mode);

    *_errno() = EINVAL;
    return -1;
}
