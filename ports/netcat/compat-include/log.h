#ifndef NETCAT_COMPAT_LOG_H
#define NETCAT_COMPAT_LOG_H

#include <err.h>

#define fatal(...) errx(1, __VA_ARGS__)

#endif
