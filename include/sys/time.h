#pragma once

#include <time.h>

#ifndef _TIMEZONE_DEFINED // FIXME
#define _TIMEZONE_DEFINED
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __MINGW32__ // FIXME
int gettimeofday(struct timeval* tv, void* tz);
#else
int gettimeofday(struct timeval* tv, struct timezone* tz);
#endif

int utimes(const char* path, const struct timeval* times);

#ifdef __cplusplus
}
#endif
