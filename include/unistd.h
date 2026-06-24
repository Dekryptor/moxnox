#pragma once

#include <sys/types.h>

#define _POSIX_THREADS 202405L

#define _SC_OPEN_MAX 4

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define F_OK 1
#define R_OK 2
#define W_OK 4
#define X_OK 8

#ifdef __cplusplus
extern "C" {
#endif

char* ttyname(int fd);
int ttyname_r(int fd, char* name, size_t namesize);
uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
int isatty(int fd);
int access(const char* path, int mode);
int close(int fd);
ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
int chdir(const char* path);
char* getcwd(char* buf, size_t size);
int unlink(const char* path);
unsigned int alarm(unsigned int seconds);
int dup(int fd);
int dup2(int oldfd, int newfd);
unsigned int sleep(unsigned int seconds);
int getpid(void);
off_t lseek(int fd, off_t offset, int whence);
int ftruncate(int fd, off_t length);
int rmdir(const char* path);
int execv(const char* path, char* const argv[]);
int execvp(const char* file, char* const argv[]);
int execve(const char* file, char* const argv[], char* const envp[]);
int execlp(const char* file, const char* arg, ...);
int execl(const char* path, const char* arg, ...);
int execle(const char* path, const char* arg, ...);
int getentropy(void *buf, size_t buflen);
int gethostname(char* name, int namelen); // FIXME - POSIX says size_t, but winsock2.h has int
int pause(void);
long sysconf(int name);
int pipe(int pipefd[2]);
int fsync(int fd);
int fchdir(int fd);
int symlink(const char* path1, const char* path2);
pid_t getppid(void);
pid_t getpgid(pid_t pid);
int tcgetpgrp(int fd);
int link(const char* oldpath, const char* newpath);
int readlink(const char* path, char* buf, size_t bufsiz);
pid_t fork(void);

#ifdef __cplusplus
}
#endif
