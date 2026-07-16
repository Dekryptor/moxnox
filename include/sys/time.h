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

int gettimeofday(struct timeval* tv, void* tz);
int utimes(const char* path, const struct timeval* times);

#ifdef __cplusplus
}
#endif
