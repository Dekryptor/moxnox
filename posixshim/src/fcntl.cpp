#include <fcntl.h>
#include <stdarg.h>

import nt;
import print;
import ucrtbase;

#undef errno
import errno;

static int getfl(int fd) {
    print("FIXME - F_GETFL({})\n", fd);

    return 0;
}

static int setfl(int fd, int arg) {
    IO_STATUS_BLOCK iosb;

    auto h = (HANDLE)_get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    // FIXME - O_APPEND

    bool nonblock = !!(arg & O_NONBLOCK);

    auto status = NtSetInformationFile(h, &iosb, &nonblock, sizeof(nonblock),
                                       FILE_INFORMATION_CLASS::MoxnoxPosixNonBlock);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

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
