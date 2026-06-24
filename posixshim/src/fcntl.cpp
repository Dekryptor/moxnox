#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>

import nt;
import print;
import ucrtbase;

#undef errno
import errno;

// ucrtbase
extern "C" bool mn_get_cloexec(int fd, bool* cloexec);
extern "C" bool mn_set_cloexec(int fd, bool cloexec);

static int getfl(int fd) {
    IO_STATUS_BLOCK iosb;
    OBJECT_BASIC_INFORMATION obi;
    nt_status status;
    uint32_t retlen;
    bool nonblock;
    int ret = 0;

    auto h = (HANDLE)_get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    status = NtQueryObject(h, OBJECT_INFORMATION_CLASS::BasicInformation,
                           &obi, sizeof(obi), &retlen);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    bool can_read = obi.GrantedAccess & FILE_READ_DATA;
    bool can_write = obi.GrantedAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA);

    if (can_read && can_write)
        ret |= O_RDWR;
    else if (can_read)
        ret |= O_RDONLY;
    else if (can_write)
        ret |= O_WRONLY;

    if (obi.GrantedAccess & FILE_APPEND_DATA)
        ret |= O_APPEND;

    status = NtQueryInformationFile(h, &iosb, &nonblock, sizeof(nonblock),
                                    FILE_INFORMATION_CLASS::MoxnoxPosixNonBlock);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    if (nonblock)
        ret |= O_NONBLOCK;

    return ret;
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

static int getfd(int fd) {
    bool cloexec;

    if (!mn_get_cloexec(fd, &cloexec)) {
        *_errno() = EBADF;
        return -1;
    }

    return cloexec ? FD_CLOEXEC : 0;
}

static int setfd(int fd, int arg) {
    if (arg & ~FD_CLOEXEC) {
        *_errno() = EINVAL;
        return -1;
    }

    if (!mn_set_cloexec(fd, arg & FD_CLOEXEC)) {
        *_errno() = EBADF;
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
            return getfd(fd);

        case F_SETFD: {
            va_start(ap, op);
            auto arg = va_arg(ap, int);
            va_end(ap);

            return setfd(fd, arg);
        }

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
