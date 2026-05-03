#pragma once

#include_next <sys/stat.h>
#include <sys/types.h>

#define S_IFLNK  0120000
#define S_ISLNK(m) (((m) & 0170000) == S_IFLNK)

#ifdef __cplusplus
extern "C" {
#endif

int lstat(const char *path, struct stat *buf);
int futimens(int fd, const struct timespec times[2]);

#ifdef __cplusplus
}
#endif
