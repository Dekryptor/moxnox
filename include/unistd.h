#pragma once

#include <sys/types.h>

typedef unsigned int uid_t;

#ifdef __cplusplus
extern "C" {
#endif

char* ttyname(int fd);
uid_t getuid(void);
unsigned int alarm(unsigned int seconds);
int dup2(int oldfd, int newfd);
unsigned int sleep(unsigned int seconds);
int getpid(void);
int ftruncate(int fd, off_t length);

#ifdef __cplusplus
}
#endif
