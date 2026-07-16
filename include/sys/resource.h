#pragma once

#include <sys/types.h>

typedef unsigned int rlim_t;

struct rlimit {
    rlim_t rlim_cur;
    rlim_t rlim_max;
};

#define RLIMIT_NOFILE 1
#define RLIMIT_DATA   2
#define RLIMIT_CORE   3

#define RLIM_INFINITY 0

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

#ifdef __cplusplus
}
#endif
