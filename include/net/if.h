#pragma once

#ifdef __cplusplus
extern "C" {
#endif

unsigned int if_nametoindex(const char *name);

#ifdef __cplusplus
}
#endif

#define IF_NAMESIZE 16
