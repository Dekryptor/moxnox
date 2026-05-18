#include <poll.h>
#include <stdint.h>
#include <limits>

import nt;
import ucrtbase;
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

    auto entries = (mn_poll_entry*)malloc(sizeof(mn_poll_entry) * nfds);
    if (!entries) {
        *_errno() = ENOMEM;
        return -1;
    }

    for (size_t i = 0; i < nfds; i++) {
        auto e = *_errno();

        auto h = (HANDLE)_get_osfhandle((int)fds[i].fd);

        if ((intptr_t)h == -1) {
            entries[i].h = 0;
            *_errno() = e;
        } else
            entries[i].h = h;

        entries[i].events = fds[i].events;
        entries[i].revents = 0;
    }

    if (timeout >= 0)
        li.QuadPart = -(int64_t)timeout * 10000; // convert ms to units of 100ns

    status = mn_poll(entries, nfds, timeout >= 0 ? &li : nullptr);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        free(entries);
        return -1;
    }

    int ret = 0;
    for (size_t i = 0; i < nfds; i++) {
        fds[i].revents = entries[i].revents;

        if (fds[i].revents != 0)
            ret++;
    }

    free(entries);

    return ret;
}
