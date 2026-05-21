#pragma once

#define NO_OLDNAMES
#include <direct.h>
#include <io.h>
#undef NO_OLDNAMES

#define isatty(fd) _isatty(fd)
#define access(fn, mode) _access(fn, mode)
#define open _open
#define close(fd) _close(fd)
#define read(fd, buf, count) _read(fd, buf, count)
#define write(fd, buf, count) _write(fd, buf, count)
#define chdir(dir) _chdir(dir)
#define getcwd(buf, size) _getcwd(buf, size)

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

#ifdef __cplusplus
}
#endif
