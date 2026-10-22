// SPDX-License-Identifier: MIT
#pragma once

#include <sys/types.h>
#include <sched.h>
#include <time.h>

#define PTHREAD_MUTEX_INITIALIZER {0}

#define PTHREAD_ONCE_INIT ((pthread_once_t)0)

#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_ERRORCHECK 1
#define PTHREAD_MUTEX_RECURSIVE 2
#define PTHREAD_MUTEX_DEFAULT PTHREAD_MUTEX_NORMAL

#define PTHREAD_CANCEL_ENABLE 0
#define PTHREAD_CANCEL_DISABLE 1

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

#define PTHREAD_COND_INITIALIZER {0}

#ifdef __cplusplus
extern "C" {
#endif

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr);
int pthread_mutex_destroy(pthread_mutex_t* mutex);
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_trylock(pthread_mutex_t* mutex);
int pthread_mutex_unlock(pthread_mutex_t* mutex);
int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr);
int pthread_cond_destroy(pthread_cond_t* cond);
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex,
                           const struct timespec* abstime);
int pthread_cond_signal(pthread_cond_t* cond);
int pthread_cond_broadcast(pthread_cond_t* cond);
int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg);
int pthread_detach(pthread_t thread);
int pthread_join(pthread_t thread, void** retval);
[[noreturn]] void pthread_exit(void* retval);
pthread_t pthread_self(void);
int pthread_key_create(pthread_key_t* key, void (*destructor)(void*));
int pthread_key_delete(pthread_key_t key);
void* pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void* value);
int pthread_mutexattr_init(pthread_mutexattr_t* attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t* attr);
int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type);
int pthread_setcancelstate(int state, int* oldstate);
int pthread_equal(pthread_t t1, pthread_t t2);
int pthread_once(pthread_once_t* once_control, void (*init_routine)(void));
int pthread_attr_init(pthread_attr_t* attr);
int pthread_attr_destroy(pthread_attr_t* attr);
int pthread_attr_setdetachstate(pthread_attr_t* attr, int detachstate);
int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_unlock(pthread_rwlock_t* rwlock);
int pthread_cancel(pthread_t thread);
int pthread_rwlock_init(pthread_rwlock_t* rwlock,
                        const pthread_rwlockattr_t* attr);
int pthread_rwlock_destroy(pthread_rwlock_t* rwlock);
int pthread_condattr_setclock(pthread_condattr_t* attr, clockid_t clock_id);
int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stacksize);
int pthread_condattr_destroy(pthread_condattr_t* attr);
int pthread_condattr_init(pthread_condattr_t* attr);
int pthread_mutex_timedlock(pthread_mutex_t* mutex,
                            const struct timespec* abstime);
int pthread_atfork(void (*prepare)(void), void (*parent)(void),
                   void (*child)(void));
int pthread_getschedparam(pthread_t thread, int* policy,
                          struct sched_param* param);
int pthread_setschedparam(pthread_t thread, int policy,
                          const struct sched_param* param);

#ifdef __cplusplus
}
#endif
