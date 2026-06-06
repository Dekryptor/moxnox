#include <stdint.h>
#include <math.h>

using maths_err_func = int (*)(struct _exception*);

static maths_err_func err_func;

extern "C"
void _fpreset() {
    // FIXME(?)
}

extern "C"
void ___setusermatherr(maths_err_func func) {
    err_func = func;
}
