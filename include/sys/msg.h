#pragma once

#include <sys/ipc.h>
#include <time.h>

typedef unsigned long msgqnum_t;
typedef unsigned long msglen_t;

struct msqid_ds {
    struct ipc_perm msg_perm;
    msgqnum_t msg_qnum;
    msglen_t msg_qbytes;
    pid_t msg_lspid;
    pid_t msg_lrpid;
    time_t msg_stime;
    time_t msg_rtime;
    time_t msg_ctime;
};

#ifdef __cplusplus
extern "C" {
#endif

int msgctl(int msqid, int cmd, struct msqid_ds* buf);
int msgget(key_t key, int msgflg);
ssize_t msgrcv(int msqid, void* msgp, size_t msgsz, long msgtyp, int msgflg);
int msgsnd(int msqid, const void* msgp, size_t msgsz, int msgflg);

#ifdef __cplusplus
}
#endif
