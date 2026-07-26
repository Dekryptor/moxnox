#include <semaphore.h>

import print;
import ucrtbase;

#undef errno
import errno;

extern "C"
int sem_init(sem_t* /*sem*/, int /*pshared*/, unsigned int /*value*/) {
    print_stderr("FIXME - sem_init\n");

    // FIXME

    *_errno() = EINVAL;
    return -1;
}

extern "C"
int sem_destroy(sem_t* /*sem*/) {
    print_stderr("FIXME - sem_destroy\n");

    // FIXME

    *_errno() = EINVAL;
    return -1;
}

extern "C"
int sem_wait(sem_t* /*sem*/) {
    print_stderr("FIXME - sem_wait\n");

    // FIXME

    *_errno() = EINVAL;
    return -1;
}

extern "C"
int sem_trywait(sem_t* /*sem*/) {
    print_stderr("FIXME - sem_trywait\n");

    // FIXME

    *_errno() = EINVAL;
    return -1;
}
