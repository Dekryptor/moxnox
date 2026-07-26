#include <semaphore.h>
#include <atomic>
#include <limits>

import print;
import ucrtbase;

#undef errno
import errno;

using namespace std;

extern "C"
int sem_init(sem_t* sem, int pshared, unsigned int value) {
    atomic_ref<uint64_t> r(sem->_x[0]);

    if (pshared) {
        print_stderr("FIXME - sem_init with pshared\n");
        // FIXME - would need named kernel objects?
        *_errno() = ENOTSUP;
        return -1;
    }

    r.store((uint64_t)value << 32);
    sem->_x[1] = 0;

    return 0;
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

extern "C"
int sem_post(sem_t* /*sem*/) {
    print_stderr("FIXME - sem_post\n");

    // FIXME

    *_errno() = EINVAL;
    return -1;
}
