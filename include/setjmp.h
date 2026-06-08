#pragma once

#include_next <setjmp.h>

#include <signal.h>

typedef struct {
    jmp_buf jmp;
    sigset_t sig;
} sigjmp_buf;

int sigsetjmp(sigjmp_buf env, int savesigs);
[[noreturn]] void siglongjmp(sigjmp_buf env, int val);
