#pragma once

#include <sys/types.h>
#include <time.h>

#define _S_IFMT 0xf000
#define _S_IFDIR 0x4000
#define _S_IFCHR 0x2000
#define _S_IFIFO 0x1000
#define _S_IFREG 0x8000
#define _S_IFBLK 0x3000
#define _S_IREAD 0x0100
#define _S_IWRITE 0x0080
#define _S_IEXEC 0x0040

#define S_IFMT _S_IFMT
#define S_IFDIR _S_IFDIR
#define S_IFCHR _S_IFCHR
#define S_IFIFO _S_IFIFO
#define S_IFREG _S_IFREG
#define S_IFBLK _S_IFBLK

#define _S_IRWXU (_S_IREAD | _S_IWRITE | _S_IEXEC)
#define _S_IXUSR _S_IEXEC
#define _S_IWUSR _S_IWRITE
#define _S_IRUSR _S_IREAD

#define S_IRWXU _S_IRWXU
#define S_IXUSR _S_IXUSR
#define S_IWUSR _S_IWUSR
#define S_IRUSR _S_IRUSR

#define S_IRGRP (S_IRUSR >> 3)
#define S_IWGRP (S_IWUSR >> 3)
#define S_IXGRP (S_IXUSR >> 3)
#define S_IRWXG (S_IRWXU >> 3)

#define S_IROTH (S_IRGRP >> 3)
#define S_IWOTH (S_IWGRP >> 3)
#define S_IXOTH (S_IXGRP >> 3)
#define S_IRWXO (S_IRWXG >> 3)

#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
};

#define _fstat _fstat64i32
#define _fstati64 _fstat64
#define _stat _stat64i32
#define _stati64 _stat64

struct _stat32 {
    dev_t st_dev;
    ino_t st_ino;
    unsigned short st_mode;
    short st_nlink;
    short st_uid;
    short st_gid;
    dev_t st_rdev;
    _off_t st_size;
    __time32_t st_atime;
    __time32_t st_mtime;
    __time32_t st_ctime;
};

struct _stat32i64 {
    dev_t st_dev;
    ino_t st_ino;
    unsigned short st_mode;
    short st_nlink;
    short st_uid;
    short st_gid;
    dev_t st_rdev;
    long long st_size;
    __time32_t st_atime;
    __time32_t st_mtime;
    __time32_t st_ctime;
};

struct _stat64i32 {
    dev_t st_dev;
    ino_t st_ino;
    unsigned short st_mode;
    short st_nlink;
    short st_uid;
    short st_gid;
    dev_t st_rdev;
    _off_t st_size;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
};

struct _stat64 {
    dev_t st_dev;
    ino_t st_ino;
    unsigned short st_mode;
    short st_nlink;
    short st_uid;
    short st_gid;
    dev_t st_rdev;
    long long st_size;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
};

#define S_IFLNK 0120000
#define S_IFSOCK 0140000

#define S_ISLNK(m) (((m) & 0170000) == S_IFLNK)
#define S_ISSOCK(m) (((m) & 0170000) == S_IFSOCK)

#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000

#define UTIME_NOW ((1L << 30) - 1)
#define UTIME_OMIT ((1L << 30) - 2)

#ifdef __cplusplus
extern "C" {
#endif

int stat(const char* path, struct stat* buf);
int fstat(int fd, struct stat* buf);
int _stat64i32(const char* path, struct _stat64i32* buf);
int _fstat64i32(int fd, struct _stat64i32* buf);
int _stat64(const char* path, struct _stat64* buf);
int _fstat64(int fd, struct _stat64* buf);
int lstat(const char* path, struct stat* buf);
int futimens(int fd, const struct timespec times[2]);
int mkdir(const char* path, mode_t mode);
int mkdirat(int fd, const char* path, mode_t mode);
int chmod(const char* path, mode_t mode);
mode_t umask(mode_t mask);
int fchmod(int fd, mode_t mode);
int utimensat(int fd, const char* path, const struct timespec* times,
              int flag);
int fchmodat(int fd, const char* path, mode_t mode, int flag);
int fstatat(int fd, const char* path, struct stat* buf, int flag);
int mkfifo(const char* path, mode_t mode);

#ifdef __cplusplus
}
#endif

#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec
