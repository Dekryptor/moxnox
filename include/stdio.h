#pragma once

#include_next <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

ssize_t getline(char** lineptr, size_t* n, FILE* stream);
void flockfile(FILE* f);
void funlockfile(FILE* f);
int getc_unlocked(FILE* f);

#ifdef __cplusplus
}
#endif
