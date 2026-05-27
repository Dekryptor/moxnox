#include <poll.h>
#include <sys/select.h>
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

        entries[i].events = (uint16_t)fds[i].events;
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
        fds[i].revents = (int16_t)entries[i].revents;

        if (fds[i].revents != 0)
            ret++;
    }

    free(entries);

    return ret;
}

extern "C"
int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
           const timeval* timeout) {
    pollfd* pfds = nullptr;
    int npfds = 0;

    if (nfds > 0) {
        pfds = (pollfd*)malloc(sizeof(pollfd) * (size_t)nfds);
        if (!pfds) {
            *_errno() = ENOMEM;
            return -1;
        }

        for (unsigned int i = 0; i < (unsigned int)nfds; i++) {
            int16_t events = 0;

            if (readfds && FD_ISSET(i, readfds))
                events |= POLLIN;

            if (writefds && FD_ISSET(i, writefds))
                events |= POLLOUT;

            if (exceptfds && FD_ISSET(i, exceptfds))
                events |= POLLPRI;

            if (events != 0) {
                pfds[npfds].fd = i;
                pfds[npfds].events = events;
                pfds[npfds].revents = 0;
                npfds++;
            }
        }
    }

    int poll_timeout;

    if (!timeout)
        poll_timeout = -1;
    else {
        uint64_t ms = ((uint64_t)timeout->tv_sec * 1000) + (uint64_t)(timeout->tv_usec / 1000);

        if (ms > INT_MAX)
            ms = INT_MAX;

        poll_timeout = (int)ms;
    }

    auto r = poll(pfds, (nfds_t)npfds, poll_timeout);

    if (r >= 0) {
        if (readfds)
            FD_ZERO(readfds);

        if (writefds)
            FD_ZERO(writefds);

        if (exceptfds)
            FD_ZERO(exceptfds);
    }

    if (r > 0) {
        r = 0;

        for (int i = 0; i < npfds; i++) {
            auto rev = pfds[i].revents;

            if (rev == 0)
                continue;

            auto fd = pfds[i].fd;

            if (readfds && (pfds[i].events & POLLIN) && (rev & (POLLIN | POLLHUP | POLLERR))) {
                FD_SET(fd, readfds);
                r++;
            }

            if (writefds && (pfds[i].events & POLLOUT) && (rev & (POLLOUT | POLLERR))) {
                FD_SET(fd, writefds);
                r++;
            }

            if (exceptfds && (pfds[i].events & POLLPRI) && (rev & POLLPRI)) {
                FD_SET(fd, exceptfds);
                r++;
            }
        }
    }

    if (pfds)
        free(pfds);

    return r;
}
