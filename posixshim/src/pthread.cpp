#include <pthread.h>
#include <errno.h>

import print;

extern "C"
int pthread_key_create(pthread_key_t* /*key*/, void (*/*destructor*/)(void*)) {
    print("FIXME - pthread_key_create\n");

    return EINVAL;
}

extern "C"
int pthread_key_delete(pthread_key_t /*key*/) {
    print("FIXME - pthread_key_delete\n");

    return EINVAL;
}

extern "C"
void* pthread_getspecific(pthread_key_t /*key*/) {
    print("FIXME - pthread_getspecific\n");

    return nullptr;
}

extern "C"
int pthread_setspecific(pthread_key_t /*key*/, const void* /*value*/) {
    print("FIXME - pthread_setspecific\n");

    return EINVAL;
}

extern "C"
int pthread_mutex_init(pthread_mutex_t* /*mutex*/, const pthread_mutexattr_t* /*attr*/) {
    print("FIXME - pthread_mutex_init\n");

    return EINVAL;
}

extern "C"
int pthread_mutex_destroy(pthread_mutex_t* /*mutex*/) {
    print("FIXME - pthread_mutex_destroy\n");

    return EINVAL;
}

extern "C"
int pthread_mutex_lock(pthread_mutex_t* /*mutex*/) {
    print("FIXME - pthread_mutex_lock\n");

    return EINVAL;
}

extern "C"
int pthread_mutex_unlock(pthread_mutex_t* /*mutex*/) {
    print("FIXME - pthread_mutex_unlock\n");

    return EINVAL;
}

extern "C"
int pthread_cond_init(pthread_cond_t* /*cond*/, const pthread_condattr_t* /*attr*/) {
    print("FIXME - pthread_cond_init\n");

    return EINVAL;
}

extern "C"
int pthread_cond_destroy(pthread_cond_t* /*cond*/) {
    print("FIXME - pthread_cond_destroy\n");

    return EINVAL;
}

extern "C"
int pthread_cond_wait(pthread_cond_t* /*cond*/, pthread_mutex_t* /*mutex*/) {
    print("FIXME - pthread_cond_wait\n");

    return EINVAL;
}

extern "C"
int pthread_cond_signal(pthread_cond_t* /*cond*/) {
    print("FIXME - pthread_cond_signal\n");

    return EINVAL;
}

extern "C"
int pthread_create(pthread_t* /*thread*/, const pthread_attr_t* /*attr*/,
                   void* (*/*start_routine*/)(void*), void* /*arg*/) {
    print("FIXME - pthread_create\n");

    return EINVAL;
}

extern "C"
int pthread_join(pthread_t /*thread*/, void** /*retval*/) {
    print("FIXME - pthread_join\n");

    return EINVAL;
}
