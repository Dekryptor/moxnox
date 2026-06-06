#pragma once

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

char* ttyname(int fd);
uid_t getuid(void);
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
int execvp(const char* file, char* const argv[]);
int execve(const char* file, char* const argv[], char* const envp[]);
int execlp(const char* file, const char* arg, ...);
int execl(const char* path, const char* arg, ...);
int getentropy(void *buf, size_t buflen);

#ifdef __cplusplus
}
#endif
