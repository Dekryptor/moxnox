#pragma once

#include <sys/types.h>

#define NO_OLDNAMES
#include <direct.h>
#include <io.h>
#undef NO_OLDNAMES

#include_next <sys/stat.h>
#include <sys/types.h>

#define S_IFLNK  0120000
#define S_ISLNK(m) (((m) & 0170000) == S_IFLNK)

#define UTIME_NOW  ((1L << 30) - 1)
#define UTIME_OMIT ((1L << 30) - 2)

#ifdef __cplusplus
extern "C" {
#endif

int lstat(const char *path, struct stat *buf);
int futimens(int fd, const struct timespec times[2]);
int mkdir(const char* path, mode_t mode);
int chmod(const char* path, mode_t mode);
mode_t umask(mode_t mask);

#ifdef __cplusplus
}
#endif
