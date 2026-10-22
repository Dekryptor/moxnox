// SPDX-License-Identifier: MIT
#pragma once

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <signal.h>

typedef size_t nfds_t;

#ifndef _WINSOCK2API_

#define POLLRDNORM 0x0100
#define POLLRDBAND 0x0200
#define POLLIN (POLLRDNORM | POLLRDBAND)
#define POLLPRI 0x0400

#define POLLWRNORM 0x0010
#define POLLOUT (POLLWRNORM)
#define POLLWRBAND 0x0020

#define POLLERR 0x0001
#define POLLHUP 0x0002
#define POLLNVAL 0x0004

struct pollfd {
    int fd;
    int16_t events;
    int16_t revents;
};

#endif

#ifdef __cplusplus
extern "C" {
#endif

int poll(struct pollfd* fds, nfds_t nfds, int timeout);
int ppoll(struct pollfd* fds, nfds_t nfds, const struct timespec* timeout,
          const sigset_t* sigmask);

#ifdef __cplusplus
}
#endif
