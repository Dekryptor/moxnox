#pragma once

#define NO_OLDNAMES
#include <direct.h>
#include <io.h>
#undef NO_OLDNAMES

#include_next <sys/stat.h>
#include <sys/types.h>

#define S_IFLNK  0120000
#define S_ISLNK(m) (((m) & 0170000) == S_IFLNK)

#ifdef __cplusplus
extern "C" {
#endif

int lstat(const char *path, struct stat *buf);
int futimens(int fd, const struct timespec times[2]);
int mkdir(const char* path, mode_t mode);
int chmod(const char* path, mode_t mode);

#ifdef __cplusplus
}
#endif
