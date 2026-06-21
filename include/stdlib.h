#pragma once

#include_next <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

char* realpath(const char* path, char* resolved);
int mkstemps(char* tmpl, int suffixlen);
int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);

#ifdef __cplusplus
}
#endif
