#pragma once

#define RTLD_NOW    1
#define RTLD_LAZY   2
#define RTLD_GLOBAL 4
#define RTLD_LOCAL  8

#ifdef __cplusplus
extern "C" {
#endif

int dlclose(void* handle);
char* dlerror(void);
void* dlopen(const char* file, int mode);
void* dlsym(void* handle, const char* name);

#ifdef __cplusplus
}
#endif
