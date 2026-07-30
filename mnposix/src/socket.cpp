#include <sys/socket.h>

import ucrtbase;

#undef errno
import errno;

extern "C"
int socketpair(int domain, int /*type*/, int /*protocol*/, int* /*socket_vector*/) {
    // FIXME - implement for AF_UNIX

    if (domain == AF_INET || domain == AF_INET6) {
        *_errno() = EOPNOTSUPP;
        return -1;
    }

    *_errno() = EAFNOSUPPORT;
    return -1;
}
