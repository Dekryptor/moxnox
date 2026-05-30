#pragma once

#include <stdint.h>
#include <sys/types.h>

typedef uint64_t sigset_t;
typedef int sig_atomic_t;

#define SA_RESETHAND  0x80000000

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

struct sigaction {
    void (*sa_handler)(int);
    sigset_t sa_mask;
    int sa_flags;
};

#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGTRAP 5
#define SIGIOT 6
#define SIGEMT 7
#define SIGFPE 8
#define SIGKILL 9
#define SIGBUS 10
#define SIGSEGV 11
#define SIGSYS 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGPOLL 16
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGBREAK 21
#define SIGABRT 22
#define SIGURG 23
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGVTALRM 26
#define SIGPROF 27
#define SIGWINCH 28
#define SIGUSR1 29
#define SIGUSR2 30
#define SIGTTIN 31
#define SIGTTOU 32

typedef void (*__sighandler_t)(int sig);

#define SIG_DFL (__sighandler_t)0
#define SIG_IGN (__sighandler_t)1
#define SIG_ERR (__sighandler_t)-1

#ifdef __cplusplus
extern "C" {
#endif

int kill(pid_t pid, int sig);
int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact);
int sigemptyset(sigset_t* set);
int sigfillset(sigset_t* set);
int sigaddset(sigset_t* set, int signum);
int sigprocmask(int how, const sigset_t* set, sigset_t* oldset);
__sighandler_t signal(int sig, __sighandler_t func);
int raise(int sig);

#ifdef __cplusplus
}
#endif
