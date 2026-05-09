#pragma once

#include <stdint.h>

/* Prevent MinGW's time.h from pulling in pthread_time.h for clock_gettime. */
#ifndef WIN_PTHREADS_TIME_H
#define WIN_PTHREADS_TIME_H
#endif

#include_next <time.h>

typedef int32_t clockid_t;

#define CLOCK_MONOTONIC 1

#ifdef __cplusplus
extern "C" {
#endif

int clock_gettime(clockid_t clock_id, struct timespec* tp);

#ifdef __cplusplus
}
#endif
