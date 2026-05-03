#pragma once

#include_next <string.h>

#ifdef __cplusplus
extern "C" {
#endif

char* strcasestr(const char* haystack, const char* needle);

#ifdef __cplusplus
}
#endif
