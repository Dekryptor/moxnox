#pragma once

#include_next <signal.h>
#include <stdint.h>
#include <sys/types.h>

typedef uint64_t sigset_t;

#define SA_RESETHAND  0x80000000

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

struct sigaction {
    void (*sa_handler)(int);
    sigset_t sa_mask;
    int sa_flags;
};

#ifdef __cplusplus
extern "C" {
#endif

int kill(pid_t pid, int sig);
int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact);
int sigemptyset(sigset_t* set);
int sigfillset(sigset_t* set);
int sigaddset(sigset_t* set, int signum);
int sigprocmask(int how, const sigset_t* set, sigset_t* oldset);

#ifdef __cplusplus
}
#endif
