#pragma once

#include <stdint.h>

typedef uint16_t sa_family_t;
typedef int socklen_t;

struct sockaddr {
   sa_family_t sa_family;
   char sa_data[14];
};

struct sockaddr_storage {
   struct sockaddr sa;
};

#define AF_UNSPEC 0
#define AF_UNIX 1
#define AF_INET 2
#define AF_IMPLINK 3
#define AF_PUP 4
#define AF_CHAOS 5
#define AF_IPX 6
#define AF_NS 6
#define AF_ISO 7
#define AF_OSI AF_ISO
#define AF_ECMA 8
#define AF_DATAKIT 9
#define AF_CCITT 10
#define AF_SNA 11
#define AF_DECnet 12
#define AF_DLI 13
#define AF_LAT 14
#define AF_HYLINK 15
#define AF_APPLETALK 16
#define AF_NETBIOS 17
#define AF_VOICEVIEW 18
#define AF_FIREFOX 19
#define AF_UNKNOWN1 20
#define AF_BAN 21
#define AF_ATM 22
#define AF_INET6 23
#define AF_CLUSTER 24
#define AF_12844 25
#define AF_IRDA 26
#define AF_NETDES 28
#define AF_TCNPROCESS 29
#define AF_TCNMESSAGE 30
#define AF_ICLFXBM 31
#define AF_BTH 32
#define AF_LINK 33
#define AF_HYPERV 34
#define AF_MAX 35

#define PF_UNSPEC AF_UNSPEC
#define PF_INET AF_INET
#define PF_INET6 AF_INET6

#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3
#define SOCK_RDM 4
#define SOCK_SEQPACKET 5

#define SOL_SOCKET 0xffff

#define SO_DEBUG 0x0001
#define SO_ACCEPTCONN 0x0002
#define SO_REUSEADDR 0x0004
#define SO_KEEPALIVE 0x0008
#define SO_DONTROUTE 0x0010
#define SO_BROADCAST 0x0020
#define SO_USELOOPBACK 0x0040
#define SO_LINGER 0x0080
#define SO_OOBINLINE 0x0100

#define SO_SNDBUF 0x1001
#define SO_RCVBUF 0x1002
#define SO_SNDLOWAT 0x1003
#define SO_RCVLOWAT 0x1004
#define SO_SNDTIMEO 0x1005
#define SO_RCVTIMEO 0x1006
#define SO_ERROR 0x1007
#define SO_TYPE 0x1008

#ifdef __cplusplus
extern "C" {
#endif

int accept(int s, struct sockaddr* addr, socklen_t* addrlen);
int bind(int s, const struct sockaddr* addr, socklen_t addrlen);
int connect(int s, const struct sockaddr* addr, socklen_t addrlen);
int getpeername(int s, struct sockaddr* name, socklen_t* namelen);
int getsockname(int s, struct sockaddr* name, socklen_t* namelen);
int getsockopt(int s, int level, int optname, char* optval, socklen_t* optlen);
int listen(int s, int backlog);
int recv(int s, char* buf, int len, int flags);
int recvfrom(int s, char* buf, int len, int flags, struct sockaddr* from,
             socklen_t* fromlen);
int send(int s, const char* buf, int len, int flags);
int sendto(int s, const char* buf, int len, int flags, const struct sockaddr* to,
           socklen_t tolen);
int setsockopt(int s, int level, int optname, const char* optval,
               socklen_t optlen);
int socket(int af, int type, int protocol);

#ifdef __cplusplus
}
#endif
