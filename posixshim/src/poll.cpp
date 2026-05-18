#include <poll.h>

import print;

#undef errno
import errno;

extern "C"
int poll(struct pollfd* fds, nfds_t nfds, int timeout) {
    print_stderr("FIXME - poll\n");

    *_errno() = EINVAL;
    return -1;
}
