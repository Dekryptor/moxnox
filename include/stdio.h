#pragma once

#include_next <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

ssize_t getline(char** lineptr, size_t* n, FILE* stream);

#ifdef __cplusplus
}
#endif
