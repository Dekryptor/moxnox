#include <string_view>
#include <optional>
#include <stdlib.h>
#include <stdint.h>

import ws2_32;
import ipv6;

using namespace std;

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

    // FIXME - resolve scope names

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
