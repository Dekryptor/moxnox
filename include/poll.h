#pragma once

#include <stddef.h>

typedef size_t nfds_t;

// FIXME - don't rely on winsock2.h's definition of struct pollfd?

int poll(struct pollfd* fds, nfds_t nfds, int timeout);
