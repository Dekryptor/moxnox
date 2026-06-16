#include <fcntl.h>
#include <stdarg.h>

import print;
import ucrtbase;

#undef errno
import errno;

static int getfl(int fd) {
    print("FIXME - F_GETFL({})\n", fd);

    return 0;
}

static int setfl(int fd, int arg) {
    print("FIXME - F_SETFL({}, {:x})\n", fd, arg);

    return 0;
}

extern "C"
int fcntl(int fd, int op, ...) {
    va_list ap;

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
            return getfl(fd);

        case F_SETFL: {
            va_start(ap, op);
            auto arg = va_arg(ap, int);
            va_end(ap);

            return setfl(fd, arg);
        }

        default:
            print("FIXME - fcntl with unknown op {}\n", op);
        break;
    }

    *_errno() = EINVAL;
    return -1;
}
