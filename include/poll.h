#pragma once

#include <stddef.h>
#include <winsock2.h>

typedef size_t nfds_t;

// FIXME - don't rely on winsock2.h's definition of struct pollfd?

#ifdef __cplusplus
extern "C" {
#endif

int poll(struct pollfd* fds, nfds_t nfds, int timeout);

#ifdef __cplusplus
}
#endif
