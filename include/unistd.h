#pragma once

#include_next <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

char* ttyname(int fd);

#ifdef __cplusplus
}
#endif
