#include <sys/socket.h>

import nt;
import ucrtbase;
import moxnox_ioctls;

#undef errno
import errno;

extern "C"
int socketpair(int domain, int /*type*/, int /*protocol*/, int* /*socket_vector*/) {
    // FIXME - implement for AF_UNIX

    if (domain == AF_INET || domain == AF_INET6) {
        *_errno() = EOPNOTSUPP;
        return -1;
    }

    *_errno() = EAFNOSUPPORT;
    return -1;
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
