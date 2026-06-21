#pragma once

#include <stdint.h>

/* Prevent MinGW's time.h from pulling in pthread_time.h for clock_gettime. */
#ifndef WIN_PTHREADS_TIME_H
#define WIN_PTHREADS_TIME_H
#endif

#include_next <time.h>

typedef int32_t clockid_t;

#define CLOCK_MONOTONIC 1
#define CLOCK_REALTIME 2

#ifdef __cplusplus
extern "C" {
#endif

int clock_gettime(clockid_t clock_id, struct timespec* tp);
int clock_getres(clockid_t clock_id, struct timespec* res);
int nanosleep(const struct timespec* duration, struct timespec* rem);

#ifdef __cplusplus
}
#endif
