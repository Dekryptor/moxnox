#pragma once

#include <sys/types.h>
#include <signal.h>
#include <sched.h>

struct _posix_spawn_file_actions_t {
    void* _ptr1;
    void* _ptr2;
};

typedef struct _posix_spawn_file_actions_t posix_spawn_file_actions_t;
typedef void* posix_spawnattr_t;

#define POSIX_SPAWN_RESETIDS      0x01
#define POSIX_SPAWN_SETPGROUP     0x02
#define POSIX_SPAWN_SETSIGDEF     0x04
#define POSIX_SPAWN_SETSIGMASK    0x08
#define POSIX_SPAWN_SETSCHEDPARAM 0x10
#define POSIX_SPAWN_SETSCHEDULER  0x20
#define POSIX_SPAWN_SETSID        0x80

#ifdef __cplusplus
extern "C" {
#endif

int posix_spawn(pid_t* pid, const char* path,
                const posix_spawn_file_actions_t* file_actions,
                const posix_spawnattr_t* attrp,
                char* const* argv, char* const* envp);
int posix_spawnp(pid_t* pid, const char* path,
                 const posix_spawn_file_actions_t* file_actions,
                 const posix_spawnattr_t* attrp,
                 char* const* argv, char* const* envp);
int posix_spawn_file_actions_init(posix_spawn_file_actions_t* file_actions);
int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t* file_actions);
int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t* file_actions,
                                     int fd, const char* path, int oflag,
                                     mode_t mode);
int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t* file_actions,
                                      int fd);
int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t* file_actions,
                                     int fd, int newfd);
int posix_spawn_file_actions_addchdir(posix_spawn_file_actions_t* file_actions,
                                      const char* path);
int posix_spawn_file_actions_addfchdir(posix_spawn_file_actions_t* file_actions,
                                       int fd);
int posix_spawnattr_init(posix_spawnattr_t* attr);
int posix_spawnattr_destroy(posix_spawnattr_t* attr);
int posix_spawnattr_setflags(posix_spawnattr_t* attr, short flags);
int posix_spawnattr_setsigdefault(posix_spawnattr_t* attr,
                                  const sigset_t* sigdefault);
int posix_spawnattr_setsigmask(posix_spawnattr_t* attr,
                               const sigset_t* sigmask);
int posix_spawnattr_setpgroup(posix_spawnattr_t* attr, pid_t pgroup);
int posix_spawnattr_setschedpolicy(posix_spawnattr_t* attr, int schedpolicy);
int posix_spawnattr_setschedparam(posix_spawnattr_t* attr,
                                  const struct sched_param* schedparam);

#ifdef __cplusplus
}
#endif
