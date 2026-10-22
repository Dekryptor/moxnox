// SPDX-License-Identifier: MIT
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int __freading(FILE* stream);
int __fwriting(FILE* stream);
size_t __fpending(FILE* stream);
void __fpurge(FILE* stream);
void __fseterr(FILE* stream);

#ifdef __cplusplus
}
#endif
