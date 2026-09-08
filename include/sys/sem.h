#pragma once

#include <sys/ipc.h>
#include <time.h>

#define SEM_UNDO 010000

#define GETPID 11
#define GETVAL 12
#define GETALL 13
#define GETNCNT 14
#define GETZCNT 15
#define SETVAL 16
#define SETALL 17

struct semid_ds {
    struct ipc_perm sem_perm;
    unsigned short sem_nsems;
    time_t sem_otime;
    time_t sem_ctime;
};

struct sembuf {
    unsigned short sem_num;
    short sem_op;
    short sem_flg;
};

#ifdef __cplusplus
extern "C" {
#endif

int semctl(int semid, int semnum, int cmd, ...);
int semget(key_t key, int nsems, int semflg);
int semop(int semid, struct sembuf* sops, size_t nsops);

#ifdef __cplusplus
}
#endif
