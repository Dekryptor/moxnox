#pragma once

#include_next <fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif

int open(const char* path, int flags, ...);

#ifdef __cplusplus
}
#endif
