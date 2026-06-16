#include <stdint.h>
#include <string.h>
#include <bit>
#include <string_view>

import nt;
import ucrtbase;
import ws2_32;
import unique_handle;
import moxnox_ioctls;
import print;

#undef errno
import errno;

static const char16_t netctl_path[] = u"/dev/netctl";

using namespace std;

extern "C"
SOCKET socket(int af, int type, int protocol) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle netctl;
    MOXNOX_NETCTL_SOCKET params;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(sizeof(netctl_path) - sizeof(char16_t));
    us.Buffer = (char16_t*)netctl_path;

    auto status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0,
                               0, FILE_OPEN, FILE_SYNCHRONOUS_IO_ALERT, nullptr,
                               0);
    if (success(status))
        netctl.reset(h);
    else {
        *_errno() = nt_status_to_errno(status);
        return INVALID_SOCKET;
    }

    params.af = af;
    params.type = type;
    params.protocol = protocol;
    params.h = &h;

    status = NtDeviceIoControlFile(netctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_NETCTL_SOCKET, &params,
                                   (uint32_t)sizeof(params), nullptr, 0);
    if (!success(status)) {
        *_errno() = EINVAL; // FIXME?
        return INVALID_SOCKET;
    }

    auto fd = _open_osfhandle((intptr_t)h, 0);
    if (fd == -1)
        return INVALID_SOCKET;

    return (SOCKET)fd;
}

extern "C"
int recvfrom(SOCKET s, char* buf, int len, int flags, sockaddr* from,
             int* fromlen) {
    IO_STATUS_BLOCK iosb;
    MOXNOX_SOCKET_RECVFROM params;
    size_t recv_bytes;

    params.buf = buf;
    params.len = len;
    params.flags = flags;
    params.from = from;
    params.fromlen = fromlen;
    params.recv_bytes = &recv_bytes;

    auto h = _get_osfhandle((int)s);
    if (h == -1)
        return SOCKET_ERROR;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_SOCKET_RECVFROM, &params,
                                        (uint32_t)sizeof(params), nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return SOCKET_ERROR;
    }

    return (int)recv_bytes;
}

extern "C"
int recv(SOCKET s, char* buf, int len, int flags) {
    return recvfrom(s, buf, len, flags, nullptr, nullptr);
}

extern "C"
int sendto(SOCKET s, const char* buf, int len, int flags, const sockaddr* to,
           int tolen) {
    IO_STATUS_BLOCK iosb;
    MOXNOX_SOCKET_SENDTO params;
    size_t sent_bytes;

    params.buf = buf;
    params.len = len;
    params.flags = flags;
    params.to = to;
    params.tolen = tolen;
    params.sent_bytes = &sent_bytes;

    auto h = _get_osfhandle((int)s);
    if (h == -1)
        return SOCKET_ERROR;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_SOCKET_SENDTO, &params,
                                        (uint32_t)sizeof(params), nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return SOCKET_ERROR;
    }

    return (int)sent_bytes;
}

extern "C"
int send(SOCKET s, const char* buf, int len, int flags) {
    return sendto(s, buf, len, flags, nullptr, 0);
}

extern "C"
int bind(SOCKET s, const sockaddr* addr, int addrlen) {
    IO_STATUS_BLOCK iosb;

    auto h = _get_osfhandle((int)s);
    if (h == -1)
        return SOCKET_ERROR;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_SOCKET_BIND, addr,
                                        (uint32_t)addrlen, nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return SOCKET_ERROR;
    }

    return 0;
}

extern "C"
int connect(SOCKET s, const sockaddr* addr, int addrlen) {
    IO_STATUS_BLOCK iosb;

    auto h = _get_osfhandle((int)s);
    if (h == -1)
        return SOCKET_ERROR;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_SOCKET_CONNECT, addr,
                                        (uint32_t)addrlen, nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return SOCKET_ERROR;
    }

    return 0;
}

extern "C"
int listen(SOCKET s, int backlog) {
    IO_STATUS_BLOCK iosb;

    auto h = _get_osfhandle((int)s);
    if (h == -1)
        return SOCKET_ERROR;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_SOCKET_LISTEN, &backlog,
                                        (uint32_t)sizeof(backlog), nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return SOCKET_ERROR;
    }

    return 0;
}

extern "C"
SOCKET accept(SOCKET s, sockaddr* addr, int* addrlen) {
    IO_STATUS_BLOCK iosb;
    MOXNOX_SOCKET_ACCEPT_IN params_in;
    MOXNOX_SOCKET_ACCEPT_OUT params_out;

    params_in.addr = addr;
    params_in.addrlen = *addrlen;

    auto h = _get_osfhandle((int)s);
    if (h == -1)
        return SOCKET_ERROR;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_SOCKET_ACCEPT, &params_in,
                                        (uint32_t)sizeof(params_in),
                                        &params_out, (uint32_t)sizeof(params_out));
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return SOCKET_ERROR;
    }

    *addrlen = params_out.addrlen;

    auto fd = _open_osfhandle((intptr_t)params_out.sock, 0);
    if (fd == -1) {
        NtClose(params_out.sock);
        return INVALID_SOCKET;
    }

    return (SOCKET)fd;
}

extern "C"
uint32_t htonl(uint32_t hostlong) {
    return byteswap(hostlong);
}

extern "C"
uint16_t htons(uint16_t hostshort) {
    return byteswap(hostshort);
}

extern "C"
uint32_t ntohl(uint32_t netlong) {
    return byteswap(netlong);
}

extern "C"
uint16_t ntohs(uint16_t netshort) {
    return byteswap(netshort);
}

extern "C"
int setsockopt(SOCKET s, int level, int optname, const char* optval,
               int optlen) {
    if (level == SOL_SOCKET) {
        auto opt = (sockopt_socket)optname;

        print("FIXME - setsockopt ({}, SOL_SOCKET, {}, {:x}, {})\n", s, opt,
              (uintptr_t)optval, optlen);
    } else if (level == IPPROTO_TCP) {
        auto opt = (sockopt_tcp)optname;

        print("FIXME - setsockopt ({}, IPPROTO_TCP, {}, {:x}, {})\n", s, opt,
              (uintptr_t)optval, optlen);
    } else {
        print("FIXME - setsockopt ({}, {}, {}, {:x}, {})\n", s, level, optname,
              (uintptr_t)optval, optlen);
    }

    return 0;
}

extern "C"
int ioctlsocket(SOCKET s, int32_t cmd, uint32_t* argp) {
    print("FIXME - ioctlsocket ({}, {}, {:x})\n", s, cmd, (uintptr_t)argp);

    return 0;
}

extern "C"
int getsockname(SOCKET s, sockaddr* name, int* namelen) {
    IO_STATUS_BLOCK iosb;
    MOXNOX_SOCKET_GETSOCKNAME params;

    if (*namelen < 0) {
        *_errno() = EINVAL;
        return SOCKET_ERROR;
    }

    auto h = _get_osfhandle((int)s);
    if (h == -1)
        return SOCKET_ERROR;

    params.addr = name;
    params.addrlen = (size_t)*namelen;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_SOCKET_GETSOCKNAME, &params,
                                        sizeof(params), namelen, sizeof(*namelen));
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return SOCKET_ERROR;
    }

    return 0;
}

extern "C"
int closesocket(SOCKET s) {
    return _close((int)s);
}

extern "C"
int gethostname(char* name, int namelen) {
    static const string_view HOSTNAME = "localhost";

    // FIXME - do this properly

    if (namelen < 0) {
        *_errno() = EINVAL;
        return -1;
    }

    if ((size_t)namelen < HOSTNAME.size() + 1) {
        *_errno() = ENAMETOOLONG;
        return -1;
    }

    memcpy(name, HOSTNAME.data(), HOSTNAME.size());
    name[HOSTNAME.size()] = 0;

    return 0;
}

extern "C"
int getsockopt(int s, int level, int optname, char* optval, socklen_t* optlen) {
    if (level == SOL_SOCKET) {
        auto opt = (sockopt_socket)optname;

        print("FIXME - getsockopt({}, SOL_SOCKET, {}, {:x}, {:x})\n",
              s, opt, (uintptr_t)optval, (uintptr_t)optlen);
    } else if (level == IPPROTO_TCP) {
        auto opt = (sockopt_tcp)optname;

        print("FIXME - getsockopt({}, IPPROTO_TCP, {}, {:x}, {:x})\n",
              s, opt, (uintptr_t)optval, (uintptr_t)optlen);
    } else {
        print("FIXME - getsockopt({}, {}, {}, {:x}, {:x})\n",
              s, level, optname, (uintptr_t)optval, (uintptr_t)optlen);
    }

    // FIXME

    *_errno() = EINVAL;
    return -1;
}

extern "C"
int DllMainCRTStartup(HANDLE /*hinstDLL*/, uint32_t /*dwReason*/,
                      void* /*lpvReserved*/) {
    return 1;
}
