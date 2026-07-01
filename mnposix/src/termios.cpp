#include <stdint.h>
#include <termios.h>

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

extern "C"
int cfsetospeed(termios* p, speed_t speed) {
    p->c_cflag &= (tcflag_t)~CBAUD;
    p->c_cflag |= speed & CBAUD;

    return 0;
}

extern "C"
speed_t cfgetispeed(const termios* p) {
    return p->c_cflag & CBAUD;
}

extern "C"
int cfsetispeed(termios* p, speed_t speed) {
    p->c_cflag &= ~CBAUD;
    p->c_cflag |= speed & CBAUD;

    return 0;
}

extern "C"
int tcflush(int fd, int queue_selector) {
    IO_STATUS_BLOCK iosb;

    auto h = (HANDLE)_get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_PTY_TCFLUSH, &queue_selector,
                                        sizeof(queue_selector), nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int tcgetwinsize(int fd, struct winsize* p) {
    MOXNOX_IOCTL_PTY_GET_SIZE size;
    IO_STATUS_BLOCK iosb;

    auto h = (HANDLE)_get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_PTY_GET_SIZE, nullptr, 0,
                                        &size, sizeof(size));
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    p->ws_col = size.cols;
    p->ws_row = size.rows;

    return 0;
}

extern "C"
int tcsetwinsize(int fd, const struct winsize* p) {
    MOXNOX_IOCTL_PTY_GET_SIZE size;
    IO_STATUS_BLOCK iosb;

    auto h = (HANDLE)_get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    size.cols = p->ws_col;
    size.rows = p->ws_row;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_PTY_SET_SIZE, &size,
                                        sizeof(size), nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int tcdrain(int /*fd*/) {
    return 0;
}

extern "C"
int tcflow(int /*fd*/, int action) {
    switch (action) {
        case TCOOFF:
        case TCOON:
        case TCIOFF:
        case TCION:
            return 0;

        default:
            *_errno() = EINVAL;
            return -1;
    }
}

extern "C"
int tcsendbreak(int /*fd*/, int /*duration*/) {
    return 0;
}
