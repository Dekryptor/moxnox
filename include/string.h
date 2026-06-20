#pragma once

#include_next <string.h>

#ifdef __cplusplus
extern "C" {
#endif

char* strcasestr(const char* haystack, const char* needle);
char* strdup(const char* s);

#ifdef __cplusplus
}
#endif
