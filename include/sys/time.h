#pragma once

#include <time.h>
#include <sys/select.h>

#ifndef _TIMEZONE_DEFINED // FIXME
#define _TIMEZONE_DEFINED
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};
#endif

#define timercmp(tvp, uvp, cmp) \
    ((tvp)->tv_sec cmp(uvp)->tv_sec || \
     ((tvp)->tv_sec == (uvp)->tv_sec && (tvp)->tv_usec cmp(uvp)->tv_usec))

#ifdef __cplusplus
extern "C" {
#endif

int gettimeofday(struct timeval* tv, void* tz);
int utimes(const char* path, const struct timeval* times);

#ifdef __cplusplus
}
#endif
