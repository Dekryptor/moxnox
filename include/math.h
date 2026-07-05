#pragma once

#define HUGE_VAL (__builtin_huge_val())
#define NAN (__builtin_nanf(""))

#define FP_NAN 0x0100
#define FP_NORMAL 0x0400
#define FP_INFINITE (FP_NAN | FP_NORMAL)
#define FP_ZERO 0x4000
#define FP_SUBNORMAL (FP_NORMAL | FP_ZERO)

#define isfinite(x) (__builtin_isfinite(x))
#define isinf(x) (__builtin_isinf_sign(x) != 0)
#define isnan(x) (__builtin_isnan(x))

#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923

typedef float float_t;
typedef double double_t;

#ifdef __cplusplus
extern "C" {
#endif

double acos(double x);
double asin(double x);
double atan(double x);
double atan2(double y, double x);
double cos(double x);
double sin(double x);
double tan(double x);
double acosh(double x);
double asinh(double x);
double atanh(double x);
double cosh(double x);
double sinh(double x);
double tanh(double x);
double exp(double x);
double exp2(double x);
double expm1(double x);
double frexp(double value, int* exp);
double ldexp(double x, int exp);
double log(double x);
double log10(double x);
double log1p(double x);
double log2(double x);
double modf(double value, double* iptr);
double scalbn(double x, int n);
double cbrt(double x);
double fabs(double x);
double hypot(double x, double y);
double pow(double x, double y);
double sqrt(double x);
double erf(double x);
double erfc(double x);
double ceil(double x);
double floor(double x);
double round(double x);
double fmod(double x, double y);
double copysign(double x, double y);
double nextafter(double x, double y);
double fma(double x, double y, double z);

float acosf(float x);
float asinf(float x);
float atanf(float x);
float atan2f(float y, float x);
float cosf(float x);
float sinf(float x);
float tanf(float x);
float acoshf(float x);
float asinhf(float x);
float atanhf(float x);
float coshf(float x);
float sinhf(float x);
float tanhf(float x);
float expf(float x);
float exp2f(float x);
float expm1f(float x);
float frexpf(float value, int* exp);
float ldexpf(float x, int exp);
float logf(float x);
float log10f(float x);
float log1pf(float x);
float log2f(float x);
float modff(float value, float* iptr);
float scalbnf(float x, int n);
float cbrtf(float x);
float fabsf(float x);
float hypotf(float x, float y);
float powf(float x, float y);
float sqrtf(float x);
float erff(float x);
float erfcf(float x);
float ceilf(float x);
float floorf(float x);
float roundf(float x);
float fmodf(float x, float y);
float copysignf(float x, float y);
float nextafterf(float x, float y);
float fmaf(float x, float y, float z);

#ifdef __cplusplus
}
#endif
