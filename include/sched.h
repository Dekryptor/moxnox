#pragma once

#define SCHED_FIFO 1
#define SCHED_RR 2
#define SCHED_OTHER 3

struct sched_param {
    int sched_priority;
};

#ifdef __cplusplus
extern "C" {
#endif

int sched_yield(void);

#ifdef __cplusplus
}
#endif
