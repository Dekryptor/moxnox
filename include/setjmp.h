#pragma once

#include_next <setjmp.h>

#include <signal.h>

typedef struct {
    jmp_buf jmp;
    sigset_t sig;
} _sigjmp_buf;

typedef _sigjmp_buf sigjmp_buf[1];

#define sigsetjmp(env, savesigs) \
    ((savesigs) ? (sigprocmask(SIG_BLOCK, NULL, &(env)->sig), _setjmp((env)->jmp, NULL)) \
                  : ((env)->sig |= (sigset_t)1 << 63, _setjmp((env)->jmp, NULL)))

#ifdef __cplusplus
extern "C" {
#endif

[[noreturn]] void siglongjmp(sigjmp_buf env, int val);

#ifdef __cplusplus
}
#endif
