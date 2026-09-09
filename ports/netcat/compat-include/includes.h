#ifndef NETCAT_COMPAT_INCLUDES_H
#define NETCAT_COMPAT_INCLUDES_H

#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>

#define HAVE_GETENTROPY 1

#define GETPGRP_VOID 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_DIRFD 1
#define HAVE_FCHMOD 1
#define HAVE_FCHMODAT 1
#define HAVE_FCHOWN 1
#define HAVE_FCHOWNAT 1
#define HAVE_FSTATAT 1
#define HAVE_GETPGID 1
#define HAVE_GETPGRP 1
#define HAVE_GETSID 1
#define HAVE_ISBLANK 1
#define HAVE_KILLPG 1
#define HAVE_LOCALTIME_R 1
#define HAVE_MBTOWC 1
#define HAVE_NANOSLEEP 1
#define HAVE_RAISE 1
#define HAVE_SETEGID 1
#define HAVE_SETEUID 1
#define HAVE_SETRESGID 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_TCGETPGRP 1
#define HAVE_TCSENDBREAK 1
#define HAVE_TRUNCATE 1
#define HAVE_UNLINKAT 1
#define HAVE_UTIMENSAT 1
#define HAVE_UTIMES 1

typedef unsigned char u_char;
typedef unsigned int u_int;

#define _PATH_TTY "/dev/tty"
#define DEF_WEAK(x) void __ssh_compat_weak_##x(void)

#define _NSIG NSIG

void bzero(void*, size_t);

uint32_t arc4random(void);
void arc4random_buf(void*, size_t);
uint32_t arc4random_uniform(uint32_t);
void explicit_bzero(void* p, size_t n);

#endif
