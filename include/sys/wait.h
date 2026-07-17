#pragma once

#include <sys/types.h>

#define WIFEXITED(s) (((s) & 0x7f) == 0)
#define WIFSIGNALED(s) (((s) & 0x7f) != 0 && ((s) & 0x7f) != 0x7f)
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WTERMSIG(s) ((s) & 0x7f)
#define WIFSTOPPED(s) ((s & 0xff) == 0x7f)
#define WSTOPSIG(s) ((s >> 8) & 0xff)
#define WIFCONTINUED(s) (s == 0xffff)

#define WCONTINUED 1
#define WNOHANG 2
#define WUNTRACED 4

#ifdef __cplusplus
extern "C" {
#endif

pid_t waitpid(pid_t pid, int* status, int options);
pid_t wait(int* status);

#ifdef __cplusplus
}
#endif
