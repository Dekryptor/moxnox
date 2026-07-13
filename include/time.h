#pragma once

#include <stddef.h>

typedef long __time32_t;
typedef long long __time64_t;
typedef __time64_t time_t;
typedef long clock_t;

#define CLOCKS_PER_SEC 1000

#ifndef _TM_DEFINED // FIXME
#define _TM_DEFINED
struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};
#endif

#ifndef _TIMEVAL_DEFINED // FIXME
#define _TIMEVAL_DEFINED
struct timeval {
    long tv_sec;
    long tv_usec;
};
#endif

#ifndef _TIMESPEC_DEFINED // FIXME
#define _TIMESPEC_DEFINED
struct timespec {
    time_t tv_sec;
    long tv_nsec;
};
#endif

typedef int clockid_t;

#define CLOCK_MONOTONIC 1
#define CLOCK_REALTIME 2

#ifdef __cplusplus
extern "C" {
#endif

clock_t clock(void);
size_t strftime(char* s, size_t maxsize, const char* format,
                const struct tm* timeptr);
char* asctime(const struct tm* timeptr);
time_t time(time_t* timer);
double difftime(time_t time1, time_t time0);
time_t mktime(struct tm* timeptr);
struct tm* gmtime(const time_t* timer);
struct tm* localtime(const time_t* timer);
char* ctime(const time_t* timer);
void tzset(void);
struct tm* gmtime_r(const time_t* timer, struct tm* result);
struct tm* localtime_r(const time_t* timer, struct tm* result);
int clock_gettime(clockid_t clock_id, struct timespec* tp);
int clock_getres(clockid_t clock_id, struct timespec* res);
int nanosleep(const struct timespec* duration, struct timespec* rem);

long* __timezone(void);

extern long _timezone;

#ifdef __cplusplus
}
#endif
