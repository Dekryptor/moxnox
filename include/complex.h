// SPDX-License-Identifier: MIT
#pragma once

#define complex _Complex

#define _Complex_I  (__extension__ 1.0iF)
#define I _Complex_I

#ifdef __cplusplus
extern "C" {
#endif

double creal(double complex z);
float crealf(float complex z);
long double creall(long double complex z);
double cimag(double complex z);
float cimagf(float complex z);
long double cimagl(long double complex z);

#ifdef __cplusplus
}
#endif
