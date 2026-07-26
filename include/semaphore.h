#pragma once

#include <stdint.h>

typedef struct { uint64_t _x[2]; } sem_t;

#ifdef __cplusplus
extern "C" {
#endif

int sem_init(sem_t* sem, int pshared, unsigned int value);
int sem_destroy(sem_t* sem);
int sem_wait(sem_t* sem);
int sem_trywait(sem_t* sem);
int sem_post(sem_t* sem);

#ifdef __cplusplus
}
#endif
