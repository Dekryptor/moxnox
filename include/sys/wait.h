#pragma once

#include <sys/types.h>

#define WIFEXITED(s) (((s) & 0x7f) == 0)
#define WIFSIGNALED(s) (((s) & 0x7f) != 0 && ((s) & 0x7f) != 0x7f)
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WTERMSIG(s) ((s) & 0x7f)

pid_t waitpid(pid_t pid, int* status, int options);
pid_t wait(int* status);
