#pragma once

#include_next <signal.h>
#include <stdint.h>

typedef uint64_t sigset_t;

#define SA_RESETHAND  0x80000000

struct sigaction {
    void (*sa_handler)(int);
    sigset_t sa_mask;
    int sa_flags;
};

#ifdef __cplusplus
extern "C" {
#endif

int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact);
int sigemptyset(sigset_t* set);

#ifdef __cplusplus
}
#endif
