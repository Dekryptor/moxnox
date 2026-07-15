#pragma once

#define FE_INEXACT 0x00000001
#define FE_UNDERFLOW 0x00000002
#define FE_OVERFLOW 0x00000004
#define FE_DIVBYZERO 0x00000008
#define FE_INVALID 0x00000010
#define FE_ALL_EXCEPT \
    (FE_DIVBYZERO | FE_INEXACT | FE_INVALID | FE_OVERFLOW | FE_UNDERFLOW)

#define FE_TONEAREST 0x00000000
#define FE_DOWNWARD 0x00000100
#define FE_UPWARD 0x00000200
#define FE_TOWARDZERO 0x00000300

typedef struct {
    unsigned long _Fe_ctl;
    unsigned long _Fe_stat;
} fenv_t;

typedef unsigned long fexcept_t;

#ifdef __cplusplus
extern "C" {
#endif

int fegetround(void);
int feraiseexcept(int excepts);
int feclearexcept(int excepts);
int fesetround(int round);
int fegetexceptflag(fexcept_t* flagp, int excepts);
int fesetexceptflag(const fexcept_t* flagp, int excepts);
int fetestexcept(int excepts);
int fegetenv(fenv_t* envp);
int feholdexcept(fenv_t* envp);
int fesetenv(const fenv_t* envp);
int feupdateenv(const fenv_t* envp);

#ifdef __cplusplus
}
#endif
