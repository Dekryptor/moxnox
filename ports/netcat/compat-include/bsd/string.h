#ifndef NETCAT_COMPAT_BSD_STRING_H
#define NETCAT_COMPAT_BSD_STRING_H

#include <stddef.h>

void explicit_bzero(void* buf, size_t len);

#endif
