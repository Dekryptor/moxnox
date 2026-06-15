#pragma once

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

#ifdef __cplusplus
extern "C" {
#endif

void freeaddrinfo(struct addrinfo* res);
const char* gai_strerror(int errcode);
int getaddrinfo(const char* node, const char* service,
                const struct addrinfo* hints, struct addrinfo** res);

#ifdef __cplusplus
}
#endif
