#include <poll.h>
#include <stdint.h>
#include <limits>

import nt;
import print;

#undef errno
import errno;

using namespace std;

extern "C"
int poll(struct pollfd* fds, nfds_t nfds, int timeout) {
    LARGE_INTEGER li;
    nt_status status;

    if (nfds == 0) {
        if (timeout == 0)
            return 0;

        if (timeout < 0)
            li.QuadPart = -numeric_limits<int64_t>::max(); // wait (effectively) forever
        else
            li.QuadPart = -(int64_t)timeout * 10000; // convert ms to units of 100ns

        status = NtDelayExecution(true, &li);
        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }

        return 0;
    }

    print_stderr("FIXME - poll\n");

    *_errno() = EINVAL;
    return -1;
}
