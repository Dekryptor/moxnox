#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>

import nt;
import ucrtbase;
import moxnox_ioctls;
import unique_handle;

#undef errno
import errno;

static const char16_t netctl_path[] = u"/dev/netctl";

extern "C"
int socketpair(int domain, int type, int protocol, int* socket_vector) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle netctl;
    MOXNOX_SOCKETPAIR_IN params_in;
    MOXNOX_SOCKETPAIR_OUT params_out;

    // FIXME - correct errno values

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(sizeof(netctl_path) - sizeof(char16_t));
    us.Buffer = (char16_t*)netctl_path;

    auto status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0,
                               0, FILE_OPEN, FILE_SYNCHRONOUS_IO_ALERT,
                               nullptr, 0);
    if (success(status))
        netctl.reset(h);
    else {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    params_in.domain = domain;
    params_in.type = type;
    params_in.protocol = protocol;

    status = NtDeviceIoControlFile(netctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_NETCTL_SOCKETPAIR, &params_in,
                                   (uint32_t)sizeof(params_in),
                                   &params_out, (uint32_t)sizeof(params_out));
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    socket_vector[0] = _open_osfhandle(params_out.h1, 0);

    if (socket_vector[0] == -1) {
        *_errno() = ENFILE;
        NtClose(params_out.h1);
        NtClose(params_out.h2);
        return -1;
    }

    socket_vector[1] = _open_osfhandle(params_out.h2, 0);

    if (socket_vector[1] == -1) {
        close(socket_vector[0]);
        *_errno() = ENFILE;
        NtClose(params_out.h2);
        return -1;
    }

    return 0;
}

extern "C"
ssize_t sendmsg(int s, const msghdr* msg, int flags) {
    IO_STATUS_BLOCK iosb;
    MOXNOX_SOCKET_SENDMSG params;
    size_t sent_bytes;

    if (!msg) {
        *_errno() = EINVAL;
        return -1;
    }

    if (msg->msg_iovlen < 0 || msg->msg_namelen < 0) {
        *_errno() = EINVAL;
        return -1;
    }

    if (msg->msg_controllen != 0) {
        *_errno() = EINVAL;
        return -1;
    }

    params.vec = msg->msg_iov;
    params.vec_count = msg->msg_iovlen;
    params.flags = flags & ~MSG_NOSIGNAL;
    params.to = msg->msg_name;
    params.tolen = msg->msg_namelen;
    params.sent_bytes = &sent_bytes;

    auto h = _get_osfhandle((int)s);
    if (h == -1)
        return -1;

    while (true) {
        auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                            IOCTL_MOXNOX_SOCKET_SENDMSG, &params,
                                            (uint32_t)sizeof(params), nullptr, 0);
        if (status == nt_status::USER_APC)
            continue;

        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }

        break;
    }

    return (ssize_t)sent_bytes;
}

extern "C"
ssize_t recvmsg(int s, msghdr* msg, int flags) {
    IO_STATUS_BLOCK iosb;
    MOXNOX_SOCKET_RECVMSG params;
    size_t recv_bytes, fromlen;

    if (!msg) {
        *_errno() = EINVAL;
        return -1;
    }

    if (msg->msg_iovlen < 0 || msg->msg_namelen < 0) {
        *_errno() = EINVAL;
        return -1;
    }

    fromlen = msg->msg_namelen;

    params.vec = msg->msg_iov;
    params.vec_count = msg->msg_iovlen;
    params.flags = flags & ~MSG_NOSIGNAL;
    params.from = msg->msg_name;
    params.fromlen = &fromlen;
    params.recv_bytes = &recv_bytes;

    auto h = _get_osfhandle((int)s);
    if (h == -1)
        return -1;

    while (true) {
        auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                            IOCTL_MOXNOX_SOCKET_RECVMSG, &params,
                                            (uint32_t)sizeof(params), nullptr, 0);
        if (status == nt_status::USER_APC)
            continue;

        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }

        break;
    }

    msg->msg_controllen = 0;
    msg->msg_namelen = (int)fromlen;
    msg->msg_flags = 0;

    return (ssize_t)recv_bytes;
}

extern "C"
servent* getservent() {
    return nullptr;
}

extern "C"
void setservent(int /*stayopen*/) {
    // nop
}

extern "C"
void endservent() {
    // nop
}

extern "C"
hostent* gethostent() {
    return nullptr;
}

extern "C"
void sethostent(int /*stayopen*/) {
    // nop
}

extern "C"
void endhostent() {
    // nop
}
