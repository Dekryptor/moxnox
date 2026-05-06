#include <stdint.h>

import nt;
import ucrtbase;
import print;
import termios;
import moxnox_ioctls;

#undef errno
import errno;

using speed_t = unsigned int;

extern "C"
int tcgetattr(int fd, termios* p) {
    IO_STATUS_BLOCK iosb;

    auto h = (HANDLE)_get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_PTY_TCGETATTR, nullptr, 0,
                                        p, sizeof(termios));
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int tcsetattr(int fd, int optional_actions, const termios* p) {
    IO_STATUS_BLOCK iosb;

    if (!p) {
        *_errno() = EINVAL;
        return -1;
    }

    auto h = (HANDLE)_get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    uint32_t code;

    switch (optional_actions) {
        case TCSANOW:
        case TCSADRAIN:
            code = IOCTL_MOXNOX_PTY_TCSETATTR;
            break;

        case TCSAFLUSH:
            code = IOCTL_MOXNOX_PTY_TCSETATTR_FLUSH;
            break;

        default:
            *_errno() = EINVAL;
            return -1;
    }

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        code, p, sizeof(termios), nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
speed_t cfgetospeed(const termios* p) {
    return p->c_cflag & CBAUD;
}
