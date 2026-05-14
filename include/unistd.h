#pragma once

#include_next <unistd.h>

typedef unsigned int uid_t;

#ifdef __cplusplus
extern "C" {
#endif

char* ttyname(int fd);
uid_t getuid(void);

#ifdef __cplusplus
}
#endif
