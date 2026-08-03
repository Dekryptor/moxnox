#pragma once

#include <sys/types.h>
#include <sys/time.h>

typedef unsigned int rlim_t;

struct rlimit {
    rlim_t rlim_cur;
    rlim_t rlim_max;
};

struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
};

#define RUSAGE_SELF     0
#define RUSAGE_CHILDREN (-1)

#define RLIMIT_NOFILE 1
#define RLIMIT_DATA   2
#define RLIMIT_CORE   3
#define RLIMIT_CPU    4
#define RLIMIT_FSIZE  5
#define RLIMIT_STACK  6
#define RLIMIT_AS     7

#define RLIM_INFINITY ((rlim_t)-1)

#define RLIM_SAVED_MAX  RLIM_INFINITY
#define RLIM_SAVED_CUR  RLIM_INFINITY

#define PRIO_PROCESS  0

#ifdef __cplusplus
extern "C" {
#endif

int getrlimit(int resource, struct rlimit* rlp);
int setrlimit(int resource, const struct rlimit* rlp);
int getpriority(int which, id_t who);
int setpriority(int which, id_t who, int value);
int getrusage(int who, struct rusage* r_usage);

#ifdef __cplusplus
}
#endif
