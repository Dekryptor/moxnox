#include <string_view>
#include <span>
#include <stdint.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <fcntl.h>

import nt;
import ucrtbase;
import print;
import termios;
import moxnox_ioctls;
import unicode;

#undef errno
import errno;

using namespace std;

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
    p->c_cflag &= (tcflag_t)~CBAUD;
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

extern "C"
int tcsetpgrp(int fd, pid_t pgid) {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    auto pgid2 = (uintptr_t)pgid;

    // FIXME - should fail if caller not in same session as PG

    auto h = (HANDLE)_get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    status = NtDeviceIoControlFile(h, 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_PTY_SET_FOREGROUND_PROCESS_GROUP,
                                   &pgid2, sizeof(pgid2), nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int tcgetpgrp(int fd) {
    uintptr_t pgid;
    IO_STATUS_BLOCK iosb;

    auto h = (HANDLE)_get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    /* FIXME: "If there is no foreground process group, tcgetpgrp() shall return
               a value greater than 1 that does not match the process group ID
               of any existing process group." */

    auto status = NtDeviceIoControlFile(h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_PTY_GET_FOREGROUND_PROCESS_GROUP,
                                        nullptr, 0, &pgid, sizeof(pgid));
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return (int)pgid;
}

extern "C"
int posix_openpt(int oflag) {
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE ptyctl;
    MOXNOX_IOCTL_PTYCTL_CREATE_PTY_IN ptyparams;
    MOXNOX_IOCTL_PTYCTL_CREATE_PTY_OUT ptyinfo;
    int flags = 0;

    if (oflag & O_CLOEXEC)
        flags |= O_CLOEXEC;

    static const u16string_view ptyctl_fn = u"/dev/ptyctl";

    us.Length = us.MaximumLength = (uint16_t)(ptyctl_fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)ptyctl_fn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtOpenFile(&ptyctl, FILE_READ_DATA | FILE_WRITE_DATA, &oa, &iosb, 0, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    ptyparams.cols = 80;
    ptyparams.rows = 25;

    status = NtDeviceIoControlFile(ptyctl, 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_PTYCTL_CREATE_PTY, &ptyparams,
                                   sizeof(ptyparams), &ptyinfo, sizeof(ptyinfo));
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        NtClose(ptyctl);
        return -1;
    }

    NtClose(ptyctl);
    NtClose(ptyinfo.client_handle);

    auto fd = _open_osfhandle(ptyinfo.server_handle, flags);

    if (fd == -1) {
        NtClose(ptyinfo.server_handle);
        return -1;
    }

    return fd;
}

extern "C"
int ptsname_r(int fd, char* name, size_t namesize) {
    alignas(OBJECT_NAME_INFORMATION) char buf[sizeof(OBJECT_NAME_INFORMATION) + (100 * sizeof(char16_t))];
    uint32_t retlen;
    nt_status status;
    IO_STATUS_BLOCK iosb;
    HANDLE server, client;

    server = _get_osfhandle(fd);
    if ((intptr_t)server < 0)
        return *_errno();

    status = NtDeviceIoControlFile(server, 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_PTY_GET_CLIENT, nullptr, 0,
                                   &client, sizeof(client));
    if (!success(status))
        return nt_status_to_errno(status);

    status = NtQueryObject(client, OBJECT_INFORMATION_CLASS::NameInformation,
                           buf, sizeof(buf), &retlen);
    if (!success(status)) {
        NtClose(client);
        return nt_status_to_errno(status);
    }

    auto& oni = *(OBJECT_NAME_INFORMATION*)buf;
    auto u16sv = u16string_view(oni.Name.Buffer, oni.Name.Length / sizeof(char16_t));

    auto u8len = utf16_to_utf8_len(u16sv);

    if (namesize < u8len + 1) {
        NtClose(client);
        return ERANGE;
    }

    utf16_to_utf8(span(name, namesize), u16sv);
    name[u8len] = 0;

    NtClose(client);

    return 0;
}

extern "C"
char* ptsname(int fd) {
    static char buf[100];

    auto ret = ptsname_r(fd, buf, sizeof(buf));
    if (ret) {
        *_errno() = ret;
        return nullptr;
    }

    return buf;
}

extern "C"
int grantpt(int /*fd*/) {
    // nop

    return 0;
}
