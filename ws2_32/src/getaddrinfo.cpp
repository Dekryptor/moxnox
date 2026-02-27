#include <string_view>
#include <optional>
#include <bit>
#include <span>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

import nt;
import moxnox_ioctls;
import unique_handle;
import ws2_32;
import ipv6;
import print;

#undef errno
import errno;

using namespace std;

struct dns_header {
    uint16_t id;
    uint16_t recursion_desired : 1;
    uint16_t truncated : 1;
    uint16_t authoritative_answer : 1;
    uint16_t opcode : 4;
    uint16_t response : 1;
    uint16_t rcode : 4;
    uint16_t checking_disabled : 1;
    uint16_t authenticated_data : 1;
    uint16_t : 1;
    uint16_t recursion_available : 1;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed));

struct dns_question_suffix {
    uint16_t type;
    uint16_t dns_class;
} __attribute__((packed));

struct dns_resource_record {
    uint16_t type;
    uint16_t dns_class;
    uint32_t ttl;
    uint16_t rdlength;
} __attribute__((packed));

static const char16_t netctl_path[] = u"/dev/netctl";

static const ipv6_address DNS_SERVER = parse_ipv6_address("fd14:7420:2b17::1"); // FIXME
static const uint16_t DNS_PORT = 53;

static constexpr auto DNS_AAAA = byteswap((uint16_t)28);

static constexpr auto DNS_IN = byteswap((uint16_t)1);

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
        ret += (unsigned int)(c - '0');
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
        ret += (unsigned int)(c - '0');
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

static size_t get_encoded_name_len(string_view name) {
    size_t ret = 1;

    while (true) {
        auto dot = name.find('.');

        ret++;

        if (dot == string_view::npos) {
            ret += name.size();
            break;
        }

        ret += dot;
        name = string_view(name.data() + dot + 1, name.size() - dot - 1);
    }

    return ret;
}

static void get_encoded_name(string_view name, uint8_t* buf) {
    while (true) {
        auto dot = name.find('.');

        if (dot == string_view::npos) {
            *buf = (uint8_t)name.size();
            buf++;

            memcpy(buf, name.data(), name.size());
            buf += name.size();

            break;
        }

        *buf = (uint8_t)dot;
        buf++;

        memcpy(buf, name.data(), dot);
        buf += dot;

        name = string_view(name.data() + dot + 1, name.size() - dot - 1);
    }

    *buf = 0;
}

static span<const uint8_t> get_dns_name(span<uint8_t>& ans, span<uint8_t> pack) {
    span<uint8_t> sp;

    if ((ans[0] & 0xc0) == 0xc0) { // name compression
        if (ans.size() < sizeof(uint16_t))
            return {};

        auto val = (uint16_t)(byteswap(*(uint16_t*)ans.data()) & 0x3fff);

        if (val > pack.size())
            return {};

        sp = span(pack.data() + val, pack.size() - val);
        ans = span(ans.data() + sizeof(uint16_t), ans.size() - sizeof(uint16_t));
    } else
        sp = ans;

    auto orig_sp = sp;

    while (true) {
        if (sp.empty())
            return {};

        auto len = sp[0];
        sp = span(sp.data() + 1, sp.size() - 1);

        if (len == 0)
            break;

        if (sp.size() < len)
            return {};

        sp = span(sp.data() + len, sp.size() - len);
    }

    if (orig_sp.data() == ans.data())
        ans = sp;

    return span(orig_sp.data(), sp.data() - orig_sp.data());
}

static int dns_lookup(string_view name, addrinfo** res, int socktype,
                      int protocol, uint16_t port) {
    int sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) {
        print_stderr("socket returned {} (errno {})\n", sock, errno_to_string_view(*_errno()));
        return EAI_SYSTEM;
    }

    // FIXME - querying for A exclusively, querying for both A and AAAA

    struct sockaddr_in6 dest;

    memset(&dest, 0, sizeof(dest));

    dest.sin6_family = AF_INET6;
    dest.sin6_port = byteswap(DNS_PORT);
    *(ipv6_address*)&dest.sin6_addr = DNS_SERVER;

    int ret = connect(sock, (struct sockaddr*)&dest, sizeof(dest));
    if (ret < 0) {
        print_stderr("connect returned {} (errno {})\n", ret, errno_to_string_view(*_errno()));
        closesocket(sock);
        return EAI_SYSTEM;
    }

    auto name_len = get_encoded_name_len(name);
    size_t question_len = name_len + sizeof(dns_question_suffix);

    size_t packlen = sizeof(dns_header) + question_len;
    auto pack = (char*)malloc(packlen);
    if (!pack) {
        print("malloc failed\n");
        closesocket(sock);
        return EAI_MEMORY;
    }

    auto& h = *(dns_header*)pack;
    memset(&h, 0, sizeof(h));

    h.id = (uint16_t)rand();
    h.recursion_desired = 1;
    h.opcode = 0;
    h.response = 0;
    h.qdcount = byteswap((uint16_t)1);

    get_encoded_name(name, (uint8_t*)pack + sizeof(dns_header));

    auto& suf = *(dns_question_suffix*)(pack + sizeof(dns_header) + name_len);
    suf.type = DNS_AAAA;
    suf.dns_class = DNS_IN;

    auto sent = send(sock, pack, (int)packlen, 0);
    if (sent < 0) {
        print_stderr("send returned {} (errno {})\n", sent, errno_to_string_view(*_errno()));
        free(pack);
        closesocket(sock);
        return EAI_SYSTEM;
    }

    // FIXME - timeouts

    // FIXME - handling CNAMEs?

    // FIXME - can have larger packets with EDNS0
    // FIXME - TCP packets

    while (true) {
        char resp[512]; // maximum size of DNS packet

        auto bytes = recv(sock, resp, sizeof(resp), 0);
        if (bytes < 0) {
            print_stderr("recv returned {} (errno {})\n", bytes, errno_to_string_view(*_errno()));
            free(pack);
            closesocket(sock);
            return EAI_SYSTEM;
        }

        if ((size_t)bytes < sizeof(dns_header))
            continue;

        auto& h2 = *(dns_header*)resp;

        // check transaction ID matches
        if (h2.id != h.id)
            continue;

        if (!h2.response) { // FIXME - wait and retry?
            free(pack);
            closesocket(sock);
            return EAI_SYSTEM;
        }

        if (h2.qdcount != h.qdcount) { // FIXME - wait and retry?
            print_stderr("DNS server returned unexpected QDCOUNT {}\n", byteswap(h2.qdcount));
            free(pack);
            closesocket(sock);
            return EAI_SYSTEM;
        }

        span sp((uint8_t*)resp + sizeof(dns_header), bytes - sizeof(dns_header));

        if (sp.size() < question_len || memcmp(sp.data(), pack + sizeof(dns_header), question_len)) { // FIXME - wait and retry?
            print_stderr("DNS server did not return question\n");
            free(pack);
            closesocket(sock);
            return EAI_SYSTEM;
        }

        if (h2.rcode == 3) { // no such name
            free(pack);
            closesocket(sock);
            return EAI_NONAME;
        } else if (h2.rcode != 0) { // FIXME - wait and retry (depending on error)?
            print_stderr("DNS server returned RCODE {}\n", +h2.rcode);
            free(pack);
            closesocket(sock);
            return EAI_SYSTEM;
        }

        if (h2.ancount == 0) { // FIXME - wait and retry?
            print_stderr("DNS server had ANCOUNT of 0\n");
            free(pack);
            closesocket(sock);
            return EAI_SYSTEM;
        }

        sp = span(sp.data() + question_len, sp.size() - question_len);

        auto num_answers = byteswap(h2.ancount);

        addrinfo* last_ai = nullptr;

        for (uint16_t i = 0; i < num_answers; i++) {
            if (sp.empty())
                break;

            auto name2 = get_dns_name(sp, span((uint8_t*)resp, bytes));
            if (name2.empty())
                break;

            if (sp.size() < sizeof(dns_resource_record))
                break;

            auto& rr = *(dns_resource_record*)sp.data();

            sp = span(sp.data() + sizeof(rr), sp.size() - sizeof(rr));

            if (sp.size() < byteswap(rr.rdlength))
                break;

            auto payload = span(sp.data(), byteswap(rr.rdlength));

            sp = span(sp.data() + payload.size(), sp.size() - payload.size());

            if (rr.type != suf.type || rr.dns_class != suf.dns_class)
                continue;

            if (name2.size() != name_len || memcmp(name2.data(), pack + sizeof(dns_header), name_len))
                continue;

            if (payload.size() != sizeof(ipv6_address))
                continue;

            auto& addr = *(ipv6_address*)payload.data();

            auto ai = (addrinfo*)malloc(sizeof(addrinfo));
            if (!ai) {
                while (*res) {
                    auto next = (*res)->ai_next;

                    free((*res)->ai_addr);
                    free(*res);
                    *res = next;
                }

                return EAI_MEMORY;
            }

            auto sock = (sockaddr_in6*)malloc(sizeof(sockaddr_in6));
            if (!sock) {
                while (*res) {
                    auto next = (*res)->ai_next;

                    free((*res)->ai_addr);
                    free(*res);
                    *res = next;
                }

                free(ai);
                return EAI_MEMORY;
            }

            sock->sin6_family = AF_INET6;
            sock->sin6_port = byteswap(port);
            sock->sin6_flowinfo = 0;
            *(ipv6_address*)sock->sin6_addr = addr;
            sock->sin6_scope_id = 0;

            ai->ai_flags = 0;
            ai->ai_family = AF_INET6;
            ai->ai_socktype = socktype;
            ai->ai_protocol = protocol;
            ai->ai_addrlen = sizeof(sockaddr_in6);
            ai->ai_addr = (sockaddr*)sock;
            ai->ai_canonname = nullptr;
            ai->ai_next = nullptr;

            if (last_ai)
                last_ai->ai_next = ai;
            else
                *res = ai;

            last_ai = ai;
        }

        if (last_ai) {
            free(pack);
            closesocket(sock);
            return 0;
        }
    }

    free(pack);
    closesocket(sock);

    return EAI_NONAME;
}

extern "C"
int getaddrinfo(const char* node, const char* service, const addrinfo* hints,
                addrinfo** res) {
    int socktype, protocol, family, flags;
    uint16_t port;

    static const int understood_flags = AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST |
                                        AI_V4MAPPED | AI_ALL | AI_ADDRCONFIG |
                                        AI_NUMERICSERV;

    // FIXME - AI_CANONNAME
    // FIXME - AI_V4MAPPED
    // FIXME - AI_ALL
    // FIXME - AI_ADDRCONFIG
    // FIXME - AI_NUMERICSERV

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

    if (flags & AI_NUMERICHOST)
        return EAI_NONAME;

    return dns_lookup(n, res, socktype, protocol, port);
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
