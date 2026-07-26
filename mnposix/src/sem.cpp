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
