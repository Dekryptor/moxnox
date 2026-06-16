#include <fcntl.h>

import print;
import ucrtbase;

#undef errno
import errno;

extern "C"
int fcntl(int /*fd*/, int op, ...) {
    switch (op) {
        case F_DUPFD:
            print("FIXME - fcntl with F_DUPFD\n");
        break;

        case F_GETFD:
            print("FIXME - fcntl with F_GETFD\n");
        break;

        case F_SETFD:
            print("FIXME - fcntl with F_SETFD\n");
        break;

        case F_GETFL:
            print("FIXME - fcntl with F_GETFL\n");
        break;

        case F_SETFL:
            print("FIXME - fcntl with F_SETFL\n");
        break;

        default:
            print("FIXME - fcntl with unknown op {}\n", op);
        break;
    }

    *_errno() = EINVAL;
    return -1;
}
