#include <string_view>
#include <optional>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

import nt;
import moxnox_ioctls;
import unique_handle;
import ws2_32;
import ipv6;
import print;

using namespace std;

static const char16_t netctl_path[] = u"/dev/netctl";

static unique_handle open_netctl() {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(sizeof(netctl_path) - sizeof(char16_t));
    us.Buffer = (char16_t*)netctl_path;

    auto status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0,
                               0, FILE_OPEN, FILE_SYNCHRONOUS_IO_ALERT, nullptr,
                               0);
    if (!success(status)) {
        print_stderr("NtCreateFile returned {}\n", status);
        return {};
    }

    return unique_handle{h};
}

static uint32_t resolve_scope(string_view s) {
    nt_status status;
    MOXNOX_NETCTL_GET_NETIF_BY_NAME p;
    IO_STATUS_BLOCK iosb;
    uint32_t id;

    if (s.size() > sizeof(p.name))
        return 0;

    auto netctl = open_netctl();
    if (!netctl)
        return 0;

    memcpy(p.name, s.data(), s.size());
    memset(p.name + s.size(), 0, sizeof(p.name) - s.size());

    status = NtDeviceIoControlFile(netctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_NETCTL_GET_NETIF_BY_NAME, &p,
                                   sizeof(p), &id, (uint32_t)sizeof(id));
    if (!success(status)) {
        print_stderr("NtDeviceIoControlFile returned {}\n", status);
        return 0;
    }

    return id;
}

static optional<uint32_t> parse_scope_id(string_view n) {
    if (n.empty())
        return 0;

    bool numeric = true;
    unsigned int ret = 0;

    for (auto c : n) {
        if (c < '0' || c > '9') {
            numeric = false;
            break;
        }

        ret *= 10;
        ret += c - '0';
    }

    if (numeric)
        return ret;

    ret = resolve_scope(n);

    if (ret != 0)
        return ret;
    else
        return nullopt;
}

static optional<uint16_t> parse_service(string_view n) {
    if (n.empty())
        return 0;

    bool numeric = true;
    unsigned int ret = 0;

    for (auto c : n) {
        if (c < '0' || c > '9') {
            numeric = false;
            break;
        }

        ret *= 10;
        ret += c - '0';
    }

    if (numeric)
        return ret;

    if (n == "http")
        return 80;
    else if (n == "https")
        return 443;
    else if (n == "ssh")
        return 22;
    else if (n == "dns")
        return 53;
    else if (n == "ftp")
        return 21;

    return nullopt;
}

extern "C"
int getaddrinfo(const char* node, const char* service, const addrinfo* hints,
                addrinfo** res) {
    int socktype, protocol, family, flags;
    uint16_t port;

    static const int understood_flags = AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST |
                                        AI_V4MAPPED | AI_ALL | AI_ADDRCONFIG |
                                        AI_NUMERICSERV;

    if (!hints) {
        socktype = 0;
        protocol = 0;
        family = AF_UNSPEC;
        flags = AI_V4MAPPED | AI_ADDRCONFIG;
    } else {
        socktype = hints->ai_socktype;
        protocol = hints->ai_protocol;
        family = hints->ai_family;
        flags = hints->ai_flags;
    }

    if (flags & ~understood_flags)
        return EAI_BADFLAGS;

    if (service) {
        if (auto opt = parse_service(service); opt.has_value())
            port = *opt;
        else
            return EAI_NONAME;
    } else
        port = 0;

    auto return_ipv6 = [socktype, protocol, res, port](const ipv6_address& addr, uint32_t scope) -> int {
        auto ret = (addrinfo*)malloc(sizeof(addrinfo));
        if (!ret)
            return EAI_MEMORY;

        auto sock = (sockaddr_in6*)malloc(sizeof(sockaddr_in6));
        if (!sock) {
            free(ret);
            return EAI_MEMORY;
        }

        sock->sin6_family = AF_INET6;
        sock->sin6_port = byteswap(port);
        sock->sin6_flowinfo = 0;
        *(ipv6_address*)sock->sin6_addr = addr;
        sock->sin6_scope_id = scope;

        ret->ai_flags = 0;
        ret->ai_family = AF_INET6;
        ret->ai_socktype = socktype;
        ret->ai_protocol = protocol;
        ret->ai_addrlen = sizeof(sockaddr_in6);
        ret->ai_addr = (sockaddr*)sock;
        ret->ai_canonname = nullptr;
        ret->ai_next = nullptr;

        *res = ret;

        return 0;
    };

    if (!node) {
        if (flags & AI_PASSIVE)
            return return_ipv6(parse_ipv6_address("::"), 0);
        else
            return return_ipv6(parse_ipv6_address("::1"), 0);
    }

    string_view n = node;

    if (family == AF_UNSPEC || family == AF_INET6) {
        string_view name, scope;
        uint32_t scope_id = 0;

        if (auto pc = n.find('%'); pc != string_view::npos) {
            name = string_view(n.data(), pc);
            scope = string_view(n.data() + pc + 1, n.size() - pc - 1);
        } else
            name = n;

        if (!scope.empty()) {
            if (auto opt = parse_scope_id(scope); opt.has_value())
                scope_id = *opt;
            else
                return EAI_NONAME;
        }

        auto opt = try_parse_ipv6_address(name);

        if (opt.has_value())
            return return_ipv6(*opt, scope_id);
    }

    // FIXME - DNS if not AI_NUMERICHOST

    return EAI_FAIL;
}

extern "C"
void freeaddrinfo(addrinfo* res) {
    while (res) {
        auto next = res->ai_next;

        if (res->ai_addr)
            free(res->ai_addr);

        if (res->ai_canonname)
            free(res->ai_canonname);

        free(res);
        res = next;
    }
}

extern "C"
const char* gai_strerror(int errcode) {
    switch (errcode) {
        case 0:
            return "Success";

        case EAI_BADFLAGS:
            return "Bad value for ai_flags";

        case EAI_NONAME:
            return "Name or service not known";

        case EAI_AGAIN:
            return "Temporary failure in name resolution";

        case EAI_FAIL:
            return "Non-recoverable failure in name resolution";

        case EAI_FAMILY:
            return "ai_family not supported";

        case EAI_SOCKTYPE:
            return "ai_socktype not supported";

        case EAI_SERVICE:
            return "Servname not supported for ai_socktype";

        case EAI_MEMORY:
            return "Memory allocation failure";

        case EAI_SYSTEM:
            return "System error";

        case EAI_OVERFLOW:
            return "Result too large for supplied buffer";

        default:
            return "?";
    }
}
