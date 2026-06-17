module;

#include <stdint.h>
#include <stddef.h>
#include <span>
#include <string_view>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

export module ws2_32;

import format;

using namespace std;

// netinet/in.h
export using ::in_addr;
export using ::sockaddr_in;
export using ::in6_addr;
export using ::sockaddr_in6;
export using ::in_addr_t;

// sys/socket.h
export using ::sa_family_t;
export using ::socklen_t;
export using ::sockaddr;
export using ::sockaddr_storage;
export using ::accept;
export using ::bind;
export using ::connect;
export using ::getpeername;
export using ::getsockname;
export using ::getsockopt;
export using ::listen;
export using ::recv;
export using ::recvfrom;
export using ::send;
export using ::sendto;
export using ::setsockopt;
export using ::socket;
export using ::linger;
export using ::shutdown;

// netdb.h
export using ::addrinfo;
export using ::freeaddrinfo;
export using ::gai_strerror;
export using ::getaddrinfo;
export using ::servent;
export using ::hostent;
export using ::getnameinfo;
export using ::getservbyname;
export using ::gethostbyname;

// arpa/inet.h
export using ::htonl;
export using ::htons;
export using ::ntohl;
export using ::ntohs;
export using ::inet_addr;
export using ::inet_ntoa;
export using ::inet_ntop;
export using ::inet_pton;

constexpr uint16_t _AF_UNSPEC = AF_UNSPEC;
constexpr uint16_t _AF_UNIX = AF_UNIX;
constexpr uint16_t _AF_INET = AF_INET;
constexpr uint16_t _AF_IMPLINK = AF_IMPLINK;
constexpr uint16_t _AF_PUP = AF_PUP;
constexpr uint16_t _AF_CHAOS = AF_CHAOS;
constexpr uint16_t _AF_IPX = AF_IPX;
constexpr uint16_t _AF_NS = AF_NS;
constexpr uint16_t _AF_ISO = AF_ISO;
constexpr uint16_t _AF_OSI = AF_OSI;
constexpr uint16_t _AF_ECMA = AF_ECMA;
constexpr uint16_t _AF_DATAKIT = AF_DATAKIT;
constexpr uint16_t _AF_CCITT = AF_CCITT;
constexpr uint16_t _AF_SNA = AF_SNA;
constexpr uint16_t _AF_DECnet = AF_DECnet;
constexpr uint16_t _AF_DLI = AF_DLI;
constexpr uint16_t _AF_LAT = AF_LAT;
constexpr uint16_t _AF_HYLINK = AF_HYLINK;
constexpr uint16_t _AF_APPLETALK = AF_APPLETALK;
constexpr uint16_t _AF_NETBIOS = AF_NETBIOS;
constexpr uint16_t _AF_VOICEVIEW = AF_VOICEVIEW;
constexpr uint16_t _AF_FIREFOX = AF_FIREFOX;
constexpr uint16_t _AF_UNKNOWN1 = AF_UNKNOWN1;
constexpr uint16_t _AF_BAN = AF_BAN;
constexpr uint16_t _AF_ATM = AF_ATM;
constexpr uint16_t _AF_INET6 = AF_INET6;
constexpr uint16_t _AF_CLUSTER = AF_CLUSTER;
constexpr uint16_t _AF_12844 = AF_12844;
constexpr uint16_t _AF_IRDA = AF_IRDA;
constexpr uint16_t _AF_NETDES = AF_NETDES;
constexpr uint16_t _AF_TCNPROCESS = AF_TCNPROCESS;
constexpr uint16_t _AF_TCNMESSAGE = AF_TCNMESSAGE;
constexpr uint16_t _AF_ICLFXBM = AF_ICLFXBM;
constexpr uint16_t _AF_BTH = AF_BTH;
constexpr uint16_t _AF_LINK = AF_LINK;
constexpr uint16_t _AF_HYPERV = AF_HYPERV;
constexpr uint16_t _AF_MAX = AF_MAX;

constexpr int _SOCK_STREAM = SOCK_STREAM;
constexpr int _SOCK_DGRAM = SOCK_DGRAM;
constexpr int _SOCK_RAW = SOCK_RAW;
constexpr int _SOCK_RDM = SOCK_RDM;
constexpr int _SOCK_SEQPACKET = SOCK_SEQPACKET;

constexpr uint8_t _IPPROTO_IP = IPPROTO_IP;
constexpr uint8_t _IPPROTO_HOPOPTS = IPPROTO_HOPOPTS;
constexpr uint8_t _IPPROTO_ICMP = IPPROTO_ICMP;
constexpr uint8_t _IPPROTO_IGMP = IPPROTO_IGMP;
constexpr uint8_t _IPPROTO_GGP = IPPROTO_GGP;
constexpr uint8_t _IPPROTO_IPV4 = IPPROTO_IPV4;
constexpr uint8_t _IPPROTO_ST = IPPROTO_ST;
constexpr uint8_t _IPPROTO_TCP = IPPROTO_TCP;
constexpr uint8_t _IPPROTO_CBT = IPPROTO_CBT;
constexpr uint8_t _IPPROTO_EGP = IPPROTO_EGP;
constexpr uint8_t _IPPROTO_IGP = IPPROTO_IGP;
constexpr uint8_t _IPPROTO_PUP = IPPROTO_PUP;
constexpr uint8_t _IPPROTO_UDP = IPPROTO_UDP;
constexpr uint8_t _IPPROTO_IDP = IPPROTO_IDP;
constexpr uint8_t _IPPROTO_RDP = IPPROTO_RDP;
constexpr uint8_t _IPPROTO_IPV6 = IPPROTO_IPV6;
constexpr uint8_t _IPPROTO_ROUTING = IPPROTO_ROUTING;
constexpr uint8_t _IPPROTO_FRAGMENT = IPPROTO_FRAGMENT;
constexpr uint8_t _IPPROTO_ESP = IPPROTO_ESP;
constexpr uint8_t _IPPROTO_AH = IPPROTO_AH;
constexpr uint8_t _IPPROTO_ICMPV6 = IPPROTO_ICMPV6;
constexpr uint8_t _IPPROTO_NONE = IPPROTO_NONE;
constexpr uint8_t _IPPROTO_DSTOPTS = IPPROTO_DSTOPTS;
constexpr uint8_t _IPPROTO_ND = IPPROTO_ND;
constexpr uint8_t _IPPROTO_ICLFXBM = IPPROTO_ICLFXBM;
constexpr uint8_t _IPPROTO_PIM = IPPROTO_PIM;
constexpr uint8_t _IPPROTO_PGM = IPPROTO_PGM;
constexpr uint8_t _IPPROTO_L2TP = IPPROTO_L2TP;
constexpr uint8_t _IPPROTO_SCTP = IPPROTO_SCTP;
constexpr uint8_t _IPPROTO_RAW = IPPROTO_RAW;

constexpr int _SOL_SOCKET = SOL_SOCKET;

constexpr int _SO_DEBUG = SO_DEBUG;
constexpr int _SO_ACCEPTCONN = SO_ACCEPTCONN;
constexpr int _SO_REUSEADDR = SO_REUSEADDR;
constexpr int _SO_KEEPALIVE = SO_KEEPALIVE;
constexpr int _SO_DONTROUTE = SO_DONTROUTE;
constexpr int _SO_BROADCAST = SO_BROADCAST;
constexpr int _SO_USELOOPBACK = SO_USELOOPBACK;
constexpr int _SO_LINGER = SO_LINGER;
constexpr int _SO_OOBINLINE = SO_OOBINLINE;
constexpr int _SO_SNDBUF = SO_SNDBUF;
constexpr int _SO_RCVBUF = SO_RCVBUF;
constexpr int _SO_SNDLOWAT = SO_SNDLOWAT;
constexpr int _SO_RCVLOWAT = SO_RCVLOWAT;
constexpr int _SO_SNDTIMEO = SO_SNDTIMEO;
constexpr int _SO_RCVTIMEO = SO_RCVTIMEO;
constexpr int _SO_ERROR = SO_ERROR;
constexpr int _SO_TYPE = SO_TYPE;

constexpr int _AI_PASSIVE = AI_PASSIVE;
constexpr int _AI_CANONNAME = AI_CANONNAME;
constexpr int _AI_NUMERICHOST = AI_NUMERICHOST;
constexpr int _AI_NUMERICSERV = AI_NUMERICSERV;
constexpr int _AI_ALL = AI_ALL;
constexpr int _AI_ADDRCONFIG = AI_ADDRCONFIG;
constexpr int _AI_V4MAPPED = AI_V4MAPPED;
constexpr int _AI_NON_AUTHORITATIVE = AI_NON_AUTHORITATIVE;
constexpr int _AI_SECURE = AI_SECURE;
constexpr int _AI_RETURN_PREFERRED_NAMES = AI_RETURN_PREFERRED_NAMES;
constexpr int _AI_FQDN = AI_FQDN;
constexpr int _AI_FILESERVER = AI_FILESERVER;
constexpr int _AI_DISABLE_IDN_ENCODING = AI_DISABLE_IDN_ENCODING;

constexpr int _EAI_BADFLAGS = EAI_BADFLAGS;
constexpr int _EAI_NONAME = EAI_NONAME;
constexpr int _EAI_AGAIN = EAI_AGAIN;
constexpr int _EAI_FAIL = EAI_FAIL;
constexpr int _EAI_FAMILY = EAI_FAMILY;
constexpr int _EAI_SOCKTYPE = EAI_SOCKTYPE;
constexpr int _EAI_SERVICE = EAI_SERVICE;
constexpr int _EAI_MEMORY = EAI_MEMORY;
constexpr int _EAI_SYSTEM = EAI_SYSTEM;
constexpr int _EAI_OVERFLOW = EAI_OVERFLOW;

constexpr int _NI_NOFQDN = NI_NOFQDN;
constexpr int _NI_NUMERICHOST = NI_NUMERICHOST;
constexpr int _NI_NAMEREQD = NI_NAMEREQD;
constexpr int _NI_NUMERICSERV = NI_NUMERICSERV;
constexpr int _NI_NUMERICSCOPE = NI_NUMERICSCOPE;
constexpr int _NI_DGRAM = NI_DGRAM;
constexpr size_t _NI_MAXHOST = NI_MAXHOST;
constexpr size_t _NI_MAXSERV = NI_MAXSERV;

constexpr int _MSG_OOB = MSG_OOB;
constexpr int _MSG_PEEK = MSG_PEEK;
constexpr int _MSG_DONTROUTE = MSG_DONTROUTE;
constexpr int _MSG_CTRUNC = MSG_CTRUNC;
constexpr int _MSG_TRUNC = MSG_TRUNC;
constexpr int _MSG_EOR = MSG_EOR;
constexpr int _MSG_WAITALL = MSG_WAITALL;
constexpr int _MSG_NOSIGNAL = MSG_NOSIGNAL;
constexpr int _MSG_CMSG_CLOEXEC = MSG_CMSG_CLOEXEC;
constexpr int _MSG_CMSG_CLOFORK = MSG_CMSG_CLOFORK;

constexpr in_addr_t _INADDR_ANY = INADDR_ANY;
constexpr in_addr_t _INADDR_BROADCAST = INADDR_BROADCAST;
constexpr in_addr_t _INADDR_LOOPBACK = INADDR_LOOPBACK;

export {

using SOCKET = int;

#undef AF_UNSPEC
#undef AF_UNIX
#undef AF_INET
#undef AF_IMPLINK
#undef AF_PUP
#undef AF_CHAOS
#undef AF_IPX
#undef AF_NS
#undef AF_ISO
#undef AF_OSI
#undef AF_ECMA
#undef AF_DATAKIT
#undef AF_CCITT
#undef AF_SNA
#undef AF_DECnet
#undef AF_DLI
#undef AF_LAT
#undef AF_HYLINK
#undef AF_APPLETALK
#undef AF_NETBIOS
#undef AF_VOICEVIEW
#undef AF_FIREFOX
#undef AF_UNKNOWN1
#undef AF_BAN
#undef AF_ATM
#undef AF_INET6
#undef AF_CLUSTER
#undef AF_12844
#undef AF_IRDA
#undef AF_NETDES
#undef AF_TCNPROCESS
#undef AF_TCNMESSAGE
#undef AF_ICLFXBM
#undef AF_BTH
#undef AF_LINK
#undef AF_HYPERV
#undef AF_MAX

constexpr uint16_t AF_UNSPEC = _AF_UNSPEC;
constexpr uint16_t AF_UNIX = _AF_UNIX;
constexpr uint16_t AF_INET = _AF_INET;
constexpr uint16_t AF_IMPLINK = _AF_IMPLINK;
constexpr uint16_t AF_PUP = _AF_PUP;
constexpr uint16_t AF_CHAOS = _AF_CHAOS;
constexpr uint16_t AF_IPX = _AF_IPX;
constexpr uint16_t AF_NS = _AF_NS;
constexpr uint16_t AF_ISO = _AF_ISO;
constexpr uint16_t AF_OSI = _AF_OSI;
constexpr uint16_t AF_ECMA = _AF_ECMA;
constexpr uint16_t AF_DATAKIT = _AF_DATAKIT;
constexpr uint16_t AF_CCITT = _AF_CCITT;
constexpr uint16_t AF_SNA = _AF_SNA;
constexpr uint16_t AF_DECnet = _AF_DECnet;
constexpr uint16_t AF_DLI = _AF_DLI;
constexpr uint16_t AF_LAT = _AF_LAT;
constexpr uint16_t AF_HYLINK = _AF_HYLINK;
constexpr uint16_t AF_APPLETALK = _AF_APPLETALK;
constexpr uint16_t AF_NETBIOS = _AF_NETBIOS;
constexpr uint16_t AF_VOICEVIEW = _AF_VOICEVIEW;
constexpr uint16_t AF_FIREFOX = _AF_FIREFOX;
constexpr uint16_t AF_UNKNOWN1 = _AF_UNKNOWN1;
constexpr uint16_t AF_BAN = _AF_BAN;
constexpr uint16_t AF_ATM = _AF_ATM;
constexpr uint16_t AF_INET6 = _AF_INET6;
constexpr uint16_t AF_CLUSTER = _AF_CLUSTER;
constexpr uint16_t AF_12844 = _AF_12844;
constexpr uint16_t AF_IRDA = _AF_IRDA;
constexpr uint16_t AF_NETDES = _AF_NETDES;
constexpr uint16_t AF_TCNPROCESS = _AF_TCNPROCESS;
constexpr uint16_t AF_TCNMESSAGE = _AF_TCNMESSAGE;
constexpr uint16_t AF_ICLFXBM = _AF_ICLFXBM;
constexpr uint16_t AF_BTH = _AF_BTH;
constexpr uint16_t AF_LINK = _AF_LINK;
constexpr uint16_t AF_HYPERV = _AF_HYPERV;
constexpr uint16_t AF_MAX = _AF_MAX;

#undef SOCK_STREAM
#undef SOCK_DGRAM
#undef SOCK_RAW
#undef SOCK_RDM
#undef SOCK_SEQPACKET

constexpr int SOCK_STREAM = _SOCK_STREAM;
constexpr int SOCK_DGRAM = _SOCK_DGRAM;
constexpr int SOCK_RAW = _SOCK_RAW;
constexpr int SOCK_RDM = _SOCK_RDM;
constexpr int SOCK_SEQPACKET = _SOCK_SEQPACKET;

#undef IPPROTO_IP
#undef IPPROTO_HOPOPTS
#undef IPPROTO_ICMP
#undef IPPROTO_IGMP
#undef IPPROTO_GGP
#undef IPPROTO_IPV4
#undef IPPROTO_ST
#undef IPPROTO_TCP
#undef IPPROTO_CBT
#undef IPPROTO_EGP
#undef IPPROTO_IGP
#undef IPPROTO_PUP
#undef IPPROTO_UDP
#undef IPPROTO_IDP
#undef IPPROTO_RDP
#undef IPPROTO_IPV6
#undef IPPROTO_ROUTING
#undef IPPROTO_FRAGMENT
#undef IPPROTO_ESP
#undef IPPROTO_AH
#undef IPPROTO_ICMPV6
#undef IPPROTO_NONE
#undef IPPROTO_DSTOPTS
#undef IPPROTO_ND
#undef IPPROTO_ICLFXBM
#undef IPPROTO_PIM
#undef IPPROTO_PGM
#undef IPPROTO_L2TP
#undef IPPROTO_SCTP
#undef IPPROTO_RAW

constexpr uint8_t IPPROTO_IP = _IPPROTO_IP;
constexpr uint8_t IPPROTO_HOPOPTS = _IPPROTO_HOPOPTS;
constexpr uint8_t IPPROTO_ICMP = _IPPROTO_ICMP;
constexpr uint8_t IPPROTO_IGMP = _IPPROTO_IGMP;
constexpr uint8_t IPPROTO_GGP = _IPPROTO_GGP;
constexpr uint8_t IPPROTO_IPV4 = _IPPROTO_IPV4;
constexpr uint8_t IPPROTO_ST = _IPPROTO_ST;
constexpr uint8_t IPPROTO_TCP = _IPPROTO_TCP;
constexpr uint8_t IPPROTO_CBT = _IPPROTO_CBT;
constexpr uint8_t IPPROTO_EGP = _IPPROTO_EGP;
constexpr uint8_t IPPROTO_IGP = _IPPROTO_IGP;
constexpr uint8_t IPPROTO_PUP = _IPPROTO_PUP;
constexpr uint8_t IPPROTO_UDP = _IPPROTO_UDP;
constexpr uint8_t IPPROTO_IDP = _IPPROTO_IDP;
constexpr uint8_t IPPROTO_RDP = _IPPROTO_RDP;
constexpr uint8_t IPPROTO_IPV6 = _IPPROTO_IPV6;
constexpr uint8_t IPPROTO_ROUTING = _IPPROTO_ROUTING;
constexpr uint8_t IPPROTO_FRAGMENT = _IPPROTO_FRAGMENT;
constexpr uint8_t IPPROTO_ESP = _IPPROTO_ESP;
constexpr uint8_t IPPROTO_AH = _IPPROTO_AH;
constexpr uint8_t IPPROTO_ICMPV6 = _IPPROTO_ICMPV6;
constexpr uint8_t IPPROTO_NONE = _IPPROTO_NONE;
constexpr uint8_t IPPROTO_DSTOPTS = _IPPROTO_DSTOPTS;
constexpr uint8_t IPPROTO_ND = _IPPROTO_ND;
constexpr uint8_t IPPROTO_ICLFXBM = _IPPROTO_ICLFXBM;
constexpr uint8_t IPPROTO_PIM = _IPPROTO_PIM;
constexpr uint8_t IPPROTO_PGM = _IPPROTO_PGM;
constexpr uint8_t IPPROTO_L2TP = _IPPROTO_L2TP;
constexpr uint8_t IPPROTO_SCTP = _IPPROTO_SCTP;
constexpr uint8_t IPPROTO_RAW = _IPPROTO_RAW;

#undef SOL_SOCKET
constexpr int SOL_SOCKET = _SOL_SOCKET;

const int INVALID_SOCKET = -1;
const int SOCKET_ERROR = -1;

#undef AI_PASSIVE
#undef AI_CANONNAME
#undef AI_NUMERICHOST
#undef AI_NUMERICSERV
#undef AI_ALL
#undef AI_ADDRCONFIG
#undef AI_V4MAPPED
#undef AI_NON_AUTHORITATIVE
#undef AI_SECURE
#undef AI_RETURN_PREFERRED_NAMES
#undef AI_FQDN
#undef AI_FILESERVER
#undef AI_DISABLE_IDN_ENCODING

constexpr int AI_PASSIVE = _AI_PASSIVE;
constexpr int AI_CANONNAME = _AI_CANONNAME;
constexpr int AI_NUMERICHOST = _AI_NUMERICHOST;
constexpr int AI_NUMERICSERV = _AI_NUMERICSERV;
constexpr int AI_ALL = _AI_ALL;
constexpr int AI_ADDRCONFIG = _AI_ADDRCONFIG;
constexpr int AI_V4MAPPED = _AI_V4MAPPED;
constexpr int AI_NON_AUTHORITATIVE = _AI_NON_AUTHORITATIVE;
constexpr int AI_SECURE = _AI_SECURE;
constexpr int AI_RETURN_PREFERRED_NAMES = _AI_RETURN_PREFERRED_NAMES;
constexpr int AI_FQDN = _AI_FQDN;
constexpr int AI_FILESERVER = _AI_FILESERVER;
constexpr int AI_DISABLE_IDN_ENCODING = _AI_DISABLE_IDN_ENCODING;

#undef EAI_BADFLAGS
#undef EAI_NONAME
#undef EAI_AGAIN
#undef EAI_FAIL
#undef EAI_FAMILY
#undef EAI_SOCKTYPE
#undef EAI_SERVICE
#undef EAI_MEMORY
#undef EAI_SYSTEM
#undef EAI_OVERFLOW

constexpr int EAI_BADFLAGS = _EAI_BADFLAGS;
constexpr int EAI_NONAME = _EAI_NONAME;
constexpr int EAI_AGAIN = _EAI_AGAIN;
constexpr int EAI_FAIL = _EAI_FAIL;
constexpr int EAI_FAMILY = _EAI_FAMILY;
constexpr int EAI_SOCKTYPE = _EAI_SOCKTYPE;
constexpr int EAI_SERVICE = _EAI_SERVICE;
constexpr int EAI_MEMORY = _EAI_MEMORY;
constexpr int EAI_SYSTEM = _EAI_SYSTEM;
constexpr int EAI_OVERFLOW = _EAI_OVERFLOW;

#undef SO_DEBUG
#undef SO_ACCEPTCONN
#undef SO_REUSEADDR
#undef SO_KEEPALIVE
#undef SO_DONTROUTE
#undef SO_BROADCAST
#undef SO_USELOOPBACK
#undef SO_LINGER
#undef SO_OOBINLINE
#undef SO_SNDBUF
#undef SO_RCVBUF
#undef SO_SNDLOWAT
#undef SO_RCVLOWAT
#undef SO_SNDTIMEO
#undef SO_RCVTIMEO
#undef SO_ERROR
#undef SO_TYPE

constexpr int SO_DEBUG = _SO_DEBUG;
constexpr int SO_ACCEPTCONN = _SO_ACCEPTCONN;
constexpr int SO_REUSEADDR = _SO_REUSEADDR;
constexpr int SO_KEEPALIVE = _SO_KEEPALIVE;
constexpr int SO_DONTROUTE = _SO_DONTROUTE;
constexpr int SO_BROADCAST = _SO_BROADCAST;
constexpr int SO_USELOOPBACK = _SO_USELOOPBACK;
constexpr int SO_LINGER = _SO_LINGER;
constexpr int SO_OOBINLINE = _SO_OOBINLINE;
constexpr int SO_SNDBUF = _SO_SNDBUF;
constexpr int SO_RCVBUF = _SO_RCVBUF;
constexpr int SO_SNDLOWAT = _SO_SNDLOWAT;
constexpr int SO_RCVLOWAT = _SO_RCVLOWAT;
constexpr int SO_SNDTIMEO = _SO_SNDTIMEO;
constexpr int SO_RCVTIMEO = _SO_RCVTIMEO;
constexpr int SO_ERROR = _SO_ERROR;
constexpr int SO_TYPE = _SO_TYPE;

#undef NI_NOFQDN
#undef NI_NUMERICHOST
#undef NI_NAMEREQD
#undef NI_NUMERICSERV
#undef NI_NUMERICSCOPE
#undef NI_DGRAM
#undef NI_MAXHOST
#undef NI_MAXSERV

constexpr int NI_NOFQDN = _NI_NOFQDN;
constexpr int NI_NUMERICHOST = _NI_NUMERICHOST;
constexpr int NI_NAMEREQD = _NI_NAMEREQD;
constexpr int NI_NUMERICSERV = _NI_NUMERICSERV;
constexpr int NI_NUMERICSCOPE = _NI_NUMERICSCOPE;
constexpr int NI_DGRAM = _NI_DGRAM;
constexpr size_t NI_MAXHOST = _NI_MAXHOST;
constexpr size_t NI_MAXSERV = _NI_MAXSERV;

#undef MSG_OOB
#undef MSG_PEEK
#undef MSG_DONTROUTE
#undef MSG_CTRUNC
#undef MSG_TRUNC
#undef MSG_EOR
#undef MSG_WAITALL
#undef MSG_NOSIGNAL
#undef MSG_CMSG_CLOEXEC
#undef MSG_CMSG_CLOFORK

constexpr int MSG_OOB = _MSG_OOB;
constexpr int MSG_PEEK = _MSG_PEEK;
constexpr int MSG_DONTROUTE = _MSG_DONTROUTE;
constexpr int MSG_CTRUNC = _MSG_CTRUNC;
constexpr int MSG_TRUNC = _MSG_TRUNC;
constexpr int MSG_EOR = _MSG_EOR;
constexpr int MSG_WAITALL = _MSG_WAITALL;
constexpr int MSG_NOSIGNAL = _MSG_NOSIGNAL;
constexpr int MSG_CMSG_CLOEXEC = _MSG_CMSG_CLOEXEC;
constexpr int MSG_CMSG_CLOFORK = _MSG_CMSG_CLOFORK;

#undef INADDR_ANY
#undef INADDR_BROADCAST
#undef INADDR_LOOPBACK

constexpr in_addr_t INADDR_ANY = _INADDR_ANY;
constexpr in_addr_t INADDR_BROADCAST = _INADDR_BROADCAST;
constexpr in_addr_t INADDR_LOOPBACK = _INADDR_LOOPBACK;

enum class sockopt_socket {
    DEBUG = SO_DEBUG,
    ACCEPTCONN = SO_ACCEPTCONN,
    REUSEADDR = SO_REUSEADDR,
    KEEPALIVE = SO_KEEPALIVE,
    DONTROUTE = SO_DONTROUTE,
    BROADCAST = SO_BROADCAST,
    USELOOPBACK = SO_USELOOPBACK,
    LINGER = SO_LINGER,
    OOBINLINE = SO_OOBINLINE,
    SNDBUF = SO_SNDBUF,
    RCVBUF = SO_RCVBUF,
    SNDLOWAT = SO_SNDLOWAT,
    RCVLOWAT = SO_RCVLOWAT,
    SNDTIMEO = SO_SNDTIMEO,
    RCVTIMEO = SO_RCVTIMEO,
    ERROR = SO_ERROR,
    TYPE = SO_TYPE,
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

int closesocket(SOCKET s);

}

}
