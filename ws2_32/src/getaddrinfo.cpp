#include <string_view>
#include <stdlib.h>

import ws2_32;
import ipv6;
import print;

using namespace std;

extern "C"
int getaddrinfo(const char* node, const char* service, const addrinfo* hints,
                addrinfo** res) {
    int socktype, protocol, family, flags;

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

    if (!node) {
        // FIXME - if node is NULL and no AI_PASSIVE, return loopback address
        // FIXME - if node is NULL and AI_PASSIVE, return wildcard address

        return EAI_FAIL;
    }

    string_view n = node;

    if (family == AF_UNSPEC || family == AF_INET6) {
        auto opt = try_parse_ipv6_address(n);

        if (opt.has_value()) {
            auto ret = (addrinfo*)malloc(sizeof(addrinfo));
            if (!ret)
                return EAI_MEMORY;

            auto sock = (sockaddr_in6*)malloc(sizeof(sockaddr_in6));
            if (!sock) {
                free(ret);
                return EAI_MEMORY;
            }

            sock->sin6_family = AF_INET6;
            sock->sin6_port = 0;
            sock->sin6_flowinfo = 0;
            *(ipv6_address*)sock->sin6_addr = opt.value();
            sock->sin6_scope_id = 0;

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
        }
    }

    // FIXME - DNS if not AI_NUMERICHOST

    return EAI_FAIL;
}

extern "C"
void freeaddrinfo(addrinfo* res) {
    print_stderr("FIXME - freeaddrinfo\n");
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
