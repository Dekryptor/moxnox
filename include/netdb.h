#pragma once

#include <stdint.h>

#include <stddef.h>
#include <sys/socket.h>

struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    char* ai_canonname;
    struct sockaddr* ai_addr;
    struct addrinfo* ai_next;
};

struct servent {
    char* s_name;
    char** s_aliases;
    char* s_proto;
    int s_port;
};

struct hostent {
    char* h_name;
    char** h_aliases;
    int h_addrtype;
    int h_length;
    char** h_addr_list;
};

struct netent {
    char* n_name;
    char** n_aliases;
    int n_addrtype;
    uint32_t n_net;
};

struct protoent {
    char* p_name;
    char** p_aliases;
    int p_proto;
};

#define IPPORT_RESERVED 1024

#define EAI_BADFLAGS -1
#define EAI_NONAME -2
#define EAI_AGAIN -3
#define EAI_FAIL -4
#define EAI_FAMILY -6
#define EAI_SOCKTYPE -7
#define EAI_SERVICE -8
#define EAI_MEMORY -10
#define EAI_SYSTEM -11
#define EAI_OVERFLOW -12

#define AI_PASSIVE 0x00000001
#define AI_CANONNAME 0x00000002
#define AI_NUMERICHOST 0x00000004
#define AI_NUMERICSERV 0x00000008
#define AI_ALL 0x00000100
#define AI_ADDRCONFIG 0x00000400
#define AI_V4MAPPED 0x00000800
#define AI_NON_AUTHORITATIVE 0x00004000
#define AI_SECURE 0x00008000
#define AI_RETURN_PREFERRED_NAMES 0x00010000
#define AI_FQDN 0x00020000
#define AI_FILESERVER 0x00040000
#define AI_DISABLE_IDN_ENCODING 0x00080000

#define NI_NOFQDN (1 << 0)
#define NI_NUMERICHOST (1 << 1)
#define NI_NAMEREQD (1 << 2)
#define NI_NUMERICSERV (1 << 3)
#define NI_NUMERICSCOPE (1 << 4)
#define NI_DGRAM (1 << 5)

#define NI_MAXHOST 1025
#define NI_MAXSERV 32

#ifdef __cplusplus
extern "C" {
#endif

void freeaddrinfo(struct addrinfo* res);
const char* gai_strerror(int errcode);
int getaddrinfo(const char* node, const char* service,
                const struct addrinfo* hints, struct addrinfo** res);
int getnameinfo(const struct sockaddr* addr, socklen_t addrlen, char* host,
                socklen_t hostlen, char* serv, socklen_t servlen, int flags);
struct servent* getservbyname(const char* name, const char* proto);
struct hostent* gethostent(void);
void sethostent(int stayopen);
void endhostent(void);
struct netent* getnetent(void);
void setnetent(int stayopen);
void endnetent(void);
struct protoent* getprotoent(void);
void setprotoent(int stayopen);
void endprotoent(void);
struct servent* getservent(void);
void setservent(int stayopen);
void endservent(void);
struct netent* getnetbyaddr(uint32_t net, int type);
struct netent* getnetbyname(const char* name);
struct protoent* getprotobyname(const char* name);
struct protoent* getprotobynumber(int proto);
struct servent* getservbyport(int port, const char* proto);

#ifdef __cplusplus
}
#endif
