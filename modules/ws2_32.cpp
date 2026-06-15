module;

#include <stdint.h>
#include <stddef.h>
#include <span>
#include <string_view>

export module ws2_32;

import format;

using namespace std;

export {

using SOCKET = int;

const int AF_UNSPEC = 0;
const int AF_UNIX = 1;
const int AF_INET = 2;
const int AF_IMPLINK = 3;
const int AF_PUP = 4;
const int AF_CHAOS = 5;
const int AF_IPX = 6;
const int AF_NS = 6;
const int AF_ISO = 7;
const int AF_OSI = AF_ISO;
const int AF_ECMA = 8;
const int AF_DATAKIT = 9;
const int AF_CCITT = 10;
const int AF_SNA = 11;
const int AF_DECnet = 12;
const int AF_DLI = 13;
const int AF_LAT = 14;
const int AF_HYLINK = 15;
const int AF_APPLETALK = 16;
const int AF_NETBIOS = 17;
const int AF_VOICEVIEW = 18;
const int AF_FIREFOX = 19;
const int AF_UNKNOWN1 = 20;
const int AF_BAN = 21;
const int AF_ATM = 22;
const int AF_INET6 = 23;
const int AF_CLUSTER = 24;
const int AF_12844 = 25;
const int AF_IRDA = 26;
const int AF_NETDES = 28;
const int AF_TCNPROCESS = 29;
const int AF_TCNMESSAGE = 30;
const int AF_ICLFXBM = 31;
const int AF_BTH = 32;
const int AF_LINK = 33;
const int AF_HYPERV = 34;
const int AF_MAX = 35;

const int SOCK_STREAM = 1;
const int SOCK_DGRAM = 2;
const int SOCK_RAW = 3;
const int SOCK_RDM = 4;
const int SOCK_SEQPACKET = 5;

const int IPPROTO_IP = 0;
const int IPPROTO_HOPOPTS = 0;
const int IPPROTO_ICMP = 1;
const int IPPROTO_IGMP = 2;
const int IPPROTO_GGP = 3;
const int IPPROTO_IPV4 = 4;
const int IPPROTO_ST = 5;
const int IPPROTO_TCP = 6;
const int IPPROTO_CBT = 7;
const int IPPROTO_EGP = 8;
const int IPPROTO_IGP = 9;
const int IPPROTO_PUP = 12;
const int IPPROTO_UDP = 17;
const int IPPROTO_IDP = 22;
const int IPPROTO_RDP = 27;
const int IPPROTO_IPV6 = 41;
const int IPPROTO_ROUTING = 43;
const int IPPROTO_FRAGMENT = 44;
const int IPPROTO_ESP = 50;
const int IPPROTO_AH = 51;
const int IPPROTO_ICMPV6 = 58;
const int IPPROTO_NONE = 59;
const int IPPROTO_DSTOPTS = 60;
const int IPPROTO_ND = 77;
const int IPPROTO_ICLFXBM = 78;
const int IPPROTO_PIM = 103;
const int IPPROTO_PGM = 113;
const int IPPROTO_L2TP = 115;
const int IPPROTO_SCTP = 132;
const int IPPROTO_RAW = 255;
const int IPPROTO_MAX = 256;

const int INVALID_SOCKET = -1;
const int SOCKET_ERROR = -1;

struct sockaddr {
    uint16_t sa_family;
    char sa_data[14];
};

struct sockaddr_in6 {
    uint16_t sin6_family;
    uint16_t sin6_port;
    uint32_t sin6_flowinfo;
    uint8_t sin6_addr[16];
    uint32_t sin6_scope_id;
};

const int AI_PASSIVE = 0x00000001;
const int AI_CANONNAME = 0x00000002;
const int AI_NUMERICHOST = 0x00000004;
const int AI_NUMERICSERV = 0x00000008;
const int AI_ALL = 0x00000100;
const int AI_ADDRCONFIG = 0x00000400;
const int AI_V4MAPPED = 0x00000800;
const int AI_NON_AUTHORITATIVE = 0x00004000;
const int AI_SECURE = 0x00008000;
const int AI_RETURN_PREFERRED_NAMES = 0x00010000;
const int AI_FQDN = 0x00020000;
const int AI_FILESERVER = 0x00040000;
const int AI_DISABLE_IDN_ENCODING = 0x00080000;

const int EAI_BADFLAGS = -1;
const int EAI_NONAME = -2;
const int EAI_AGAIN = -3;
const int EAI_FAIL = -4;
const int EAI_FAMILY = -6;
const int EAI_SOCKTYPE = -7;
const int EAI_SERVICE = -8;
const int EAI_MEMORY = -10;
const int EAI_SYSTEM = -11;
const int EAI_OVERFLOW = -12;

struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    char* ai_canonname;
    sockaddr* ai_addr;
    addrinfo* ai_next;
};

// FIXME - get these constants from the header files

enum class sockopt_socket {
    DEBUG = 0x0001,
    ACCEPTCONN = 0x0002,
    REUSEADDR = 0x0004,
    KEEPALIVE = 0x0008,
    DONTROUTE = 0x0010,
    BROADCAST = 0x0020,
    USELOOPBACK = 0x0040,
    LINGER = 0x0080,
    OOBINLINE = 0x0100,
    SNDBUF = 0x1001,
    RCVBUF = 0x1002,
    SNDLOWAT = 0x1003,
    RCVLOWAT = 0x1004,
    SNDTIMEO = 0x1005,
    RCVTIMEO = 0x1006,
    ERROR = 0x1007,
    TYPE = 0x1008,
    GROUP_ID = 0x2001,
    GROUP_PRIORITY = 0x2002,
    MAX_MSG_SIZE = 0x2003,
    PROTOCOL_INFOA = 0x2004,
    PROTOCOL_INFOW = 0x2005,
};

enum class sockopt_tcp {
    OFFLOAD_NO_PREFERENCE = 0,
    OFFLOAD_NOT_PREFERRED = 1,
    OFFLOAD_PREFERRED = 2,
    KEEPALIVE = 3,
    MAXSEG = 4,
    MAXRT = 5,
    STDURG = 6,
    NOURG = 7,
    ATMARK = 8,
    NOSYNRETRIES = 9,
    TIMESTAMPS = 10,
    OFFLOAD_PREFERENCE = 11,
    CONGESTION_ALGORITHM = 12,
    DELAY_FIN_ACK = 13,
    MAXRTMS = 14,
    FASTOPEN = 15,
    KEEPCNT = 16,
    KEEPINTVL = 17,
    FAIL_CONNECT_ON_ICMP_ERROR = 18,
    ICMP_ERROR_INFO = 19,
};

template<>
consteval bool format_spec_valid<sockopt_socket>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, sockopt_socket o) {
    switch (o) {
        using enum sockopt_socket;

        case DEBUG:
            return do_format_arg(sp, "", "DEBUG");
        case ACCEPTCONN:
            return do_format_arg(sp, "", "ACCEPTCONN");
        case REUSEADDR:
            return do_format_arg(sp, "", "REUSEADDR");
        case KEEPALIVE:
            return do_format_arg(sp, "", "KEEPALIVE");
        case DONTROUTE:
            return do_format_arg(sp, "", "DONTROUTE");
        case BROADCAST:
            return do_format_arg(sp, "", "BROADCAST");
        case USELOOPBACK:
            return do_format_arg(sp, "", "USELOOPBACK");
        case LINGER:
            return do_format_arg(sp, "", "LINGER");
        case OOBINLINE:
            return do_format_arg(sp, "", "OOBINLINE");
        case SNDBUF:
            return do_format_arg(sp, "", "SNDBUF");
        case RCVBUF:
            return do_format_arg(sp, "", "RCVBUF");
        case SNDLOWAT:
            return do_format_arg(sp, "", "SNDLOWAT");
        case RCVLOWAT:
            return do_format_arg(sp, "", "RCVLOWAT");
        case SNDTIMEO:
            return do_format_arg(sp, "", "SNDTIMEO");
        case RCVTIMEO:
            return do_format_arg(sp, "", "RCVTIMEO");
        case ERROR:
            return do_format_arg(sp, "", "ERROR");
        case TYPE:
            return do_format_arg(sp, "", "TYPE");
        case GROUP_ID:
            return do_format_arg(sp, "", "GROUP_ID");
        case GROUP_PRIORITY:
            return do_format_arg(sp, "", "GROUP_PRIORITY");
        case MAX_MSG_SIZE:
            return do_format_arg(sp, "", "MAX_MSG_SIZE");
        case PROTOCOL_INFOA:
            return do_format_arg(sp, "", "PROTOCOL_INFOA");
        case PROTOCOL_INFOW:
            return do_format_arg(sp, "", "PROTOCOL_INFOW");
        default:
            return do_format_arg(sp, "", (unsigned int)o);
    }

    return sp;
}

template<>
consteval bool format_spec_valid<sockopt_tcp>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, sockopt_tcp o) {
    switch (o) {
        using enum sockopt_tcp;

        case OFFLOAD_NO_PREFERENCE:
            return do_format_arg(sp, "", "OFFLOAD_NO_PREFERENCE");
        case OFFLOAD_NOT_PREFERRED:
            return do_format_arg(sp, "", "OFFLOAD_NOT_PREFERRED");
        case OFFLOAD_PREFERRED:
            return do_format_arg(sp, "", "OFFLOAD_PREFERRED");
        case KEEPALIVE:
            return do_format_arg(sp, "", "KEEPALIVE");
        case MAXSEG:
            return do_format_arg(sp, "", "MAXSEG");
        case MAXRT:
            return do_format_arg(sp, "", "MAXRT");
        case STDURG:
            return do_format_arg(sp, "", "STDURG");
        case NOURG:
            return do_format_arg(sp, "", "NOURG");
        case ATMARK:
            return do_format_arg(sp, "", "ATMARK");
        case NOSYNRETRIES:
            return do_format_arg(sp, "", "NOSYNRETRIES");
        case TIMESTAMPS:
            return do_format_arg(sp, "", "TIMESTAMPS");
        case OFFLOAD_PREFERENCE:
            return do_format_arg(sp, "", "OFFLOAD_PREFERENCE");
        case CONGESTION_ALGORITHM:
            return do_format_arg(sp, "", "CONGESTION_ALGORITHM");
        case DELAY_FIN_ACK:
            return do_format_arg(sp, "", "DELAY_FIN_ACK");
        case MAXRTMS:
            return do_format_arg(sp, "", "MAXRTMS");
        case FASTOPEN:
            return do_format_arg(sp, "", "FASTOPEN");
        case KEEPCNT:
            return do_format_arg(sp, "", "KEEPCNT");
        case KEEPINTVL:
            return do_format_arg(sp, "", "KEEPINTVL");
        case FAIL_CONNECT_ON_ICMP_ERROR:
            return do_format_arg(sp, "", "FAIL_CONNECT_ON_ICMP_ERROR");
        case ICMP_ERROR_INFO:
            return do_format_arg(sp, "", "ICMP_ERROR_INFO");
        default:
            return do_format_arg(sp, "", (unsigned int)o);
    }

    return sp;
}

extern "C" {

SOCKET socket(int af, int type, int protocol);
int closesocket(SOCKET s);
int recv(SOCKET s, char* buf, int len, int flags);
int recvfrom(SOCKET s, char* buf, int len, int flags, sockaddr* from,
             int* fromlen);
int send(SOCKET s, const char* buf, int len, int flags);
int sendto(SOCKET s, const char* buf, int len, int flags, const sockaddr* to,
           int tolen);
int bind(SOCKET s, const sockaddr* addr, int addrlen);
int connect(SOCKET s, const sockaddr* addr, int addrlen);
int getaddrinfo(const char* node, const char* service, const addrinfo* hints,
                addrinfo** res);
void freeaddrinfo(addrinfo* res);
const char* gai_strerror(int errcode);
int setsockopt(SOCKET s, int level, int optname, const char* optval,
               int optlen);
int getsockopt(SOCKET s, int level, int optname, char* optval, int* optlen);
int listen(SOCKET s, int backlog);
SOCKET accept(SOCKET s, sockaddr* addr, int* addrlen);

}

}
