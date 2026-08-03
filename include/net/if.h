#pragma once

#ifdef __cplusplus
extern "C" {
#endif

unsigned int if_nametoindex(const char *name);
char* if_indextoname(unsigned int ifindex, char* ifname);

#ifdef __cplusplus
}
#endif

#define IF_NAMESIZE 16
