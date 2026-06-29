#include <stdint.h>
#include <string.h>
#include <bit>
#include <span>
#include <string_view>
#include <charconv>
#include <utility>

import nt;
import ucrtbase;
import ws2_32;
import unique_handle;
import moxnox_ioctls;
import print;
import format;
import ipv6;

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
    params.flags = flags & ~MSG_NOSIGNAL;
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
    params.flags = flags & ~MSG_NOSIGNAL;
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

    while (true) {
        auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                            IOCTL_MOXNOX_SOCKET_ACCEPT, &params_in,
                                            (uint32_t)sizeof(params_in),
                                            &params_out, (uint32_t)sizeof(params_out));
        if (status == nt_status::USER_APC)
            continue;

        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return SOCKET_ERROR;
        }

        break;
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
    IO_STATUS_BLOCK iosb;
    MOXNOX_SOCKET_SETSOCKOPT params;

    auto h = _get_osfhandle(s);
    if (h == -1)
        return -1;

    params.level = level;
    params.optname = optname;
    params.optval = optval;
    params.optlen = optlen;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_SOCKET_SETSOCKOPT, &params,
                                        sizeof(params), nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return SOCKET_ERROR;
    }

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
int getpeername(SOCKET s, sockaddr* name, int* namelen) {
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
                                        IOCTL_MOXNOX_SOCKET_GETPEERNAME, &params,
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
    IO_STATUS_BLOCK iosb;
    MOXNOX_SOCKET_GETSOCKOPT params;

    if (*optlen < 0) {
        *_errno() = EINVAL;
        return -1;
    }

    auto h = _get_osfhandle(s);
    if (h == -1)
        return -1;

    params.level = level;
    params.optname = optname;
    params.optval = optval;
    params.optlen = optlen;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_SOCKET_GETSOCKOPT, &params,
                                        sizeof(params), nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return SOCKET_ERROR;
    }

    return 0;
}

extern "C"
int shutdown(int s, int how) {
    IO_STATUS_BLOCK iosb;

    auto h = _get_osfhandle(s);
    if (h == -1)
        return -1;

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_SOCKET_SHUTDOWN, &how,
                                        sizeof(how), nullptr, 0);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return SOCKET_ERROR;
    }

    return 0;
}

static const char* inet_ntop_v4(const in_addr_t& src, char* dst,
                                socklen_t size) {
    if (size < 0) {
        *_errno() = ENOSPC;
        return nullptr;
    }

    span sp(dst, size);

    sp = format(sp, "{}.{}.{}.{}", (uint8_t)(src & 0xff),
                (uint8_t)((src >> 8) & 0xff), (uint8_t)((src >> 16) & 0xff),
                (uint8_t)(src >> 24));

    if (sp.empty()) {
        *_errno() = ENOSPC;
        return nullptr;
    }

    sp[0] = 0;

    return dst;
}

static const char* inet_ntop_v6(const ipv6_address& src, char* dst,
                                socklen_t size) {
    if (size < 0) {
        *_errno() = ENOSPC;
        return nullptr;
    }

    span sp(dst, size);

    sp = format(sp, "{}", src);

    if (sp.empty()) {
        *_errno() = ENOSPC;
        return nullptr;
    }

    sp[0] = 0;

    return dst;
}

extern "C"
const char* inet_ntop(int af, const void* src, char* dst, socklen_t size) {
    switch (af) {
        case AF_INET:
            return inet_ntop_v4(*(in_addr_t*)src, dst, size);

        case AF_INET6:
            return inet_ntop_v6(*(ipv6_address*)src, dst, size);

        default:
            *_errno() = EAFNOSUPPORT;
            return nullptr;
    }
}

extern "C"
in_addr_t inet_addr(const char* cp) {
    string_view sv(cp);
    uint32_t values[4];
    size_t num_parts = 0;

    // FIXME - in theory should be parsing hex or octal too

    for (size_t i = 0; i < 4; i++) {
        auto dot = sv.find('.');

        if (dot == 0)
            return (in_addr_t)-1;

        if (dot == string_view::npos)
            dot = sv.size();

        auto [ptr, ec] = from_chars(sv.data(), sv.data() + dot, values[i]);

        if (ec != errc() || ptr != sv.data() + dot)
            return (in_addr_t)-1;

        num_parts++;

        sv = string_view(sv.data() + dot + 1, sv.size() - dot - 1);
        if (sv.empty())
            break;
    }

    if (!sv.empty())
        return (in_addr_t)-1;

    if (num_parts == 1)
        return byteswap(values[0]);
    else if (num_parts == 2) {
        if (values[0] > 0xff || values[1] > 0xffffff)
            return (in_addr_t)-1;

        return byteswap(values[1]) | values[0];
    } else if (num_parts == 3) {
        if (values[0] > 0xff || values[1] > 0xff || values[2] > 0xffff)
            return (in_addr_t)-1;

        return byteswap(values[2]) | (values[1] << 8) | values[0];
    } else if (num_parts == 4) {
        for (auto v : values) {
            if (v > 0xff)
                return (in_addr_t)-1;
        }

        return (values[3] << 24) | (values[2] << 16) | (values[1] << 8) | values[0];
    } else
        unreachable();
}

extern "C"
int DllMainCRTStartup(HANDLE /*hinstDLL*/, uint32_t /*dwReason*/,
                      void* /*lpvReserved*/) {
    return 1;
}
