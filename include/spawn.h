#pragma once

#include <sys/types.h>
#include <signal.h>

struct _posix_spawn_file_actions_t {
    void* _ptr1;
    void* _ptr2;
};

typedef struct _posix_spawn_file_actions_t posix_spawn_file_actions_t;
typedef void* posix_spawnattr_t; // FIXME

#define POSIX_SPAWN_RESETIDS	1
#define POSIX_SPAWN_SETPGROUP	2
#define POSIX_SPAWN_SETSIGDEF	3
#define POSIX_SPAWN_SETSIGMASK	4

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
                               const sigset_t* restrict);

#ifdef __cplusplus
}
#endif
