#ifndef NETCAT_COMPAT_BSD_STDLIB_H
#define NETCAT_COMPAT_BSD_STDLIB_H

#include <stdint.h>

long long strtonum(const char* numstr, long long minval, long long maxval, const char** errstrp);
uint32_t arc4random(void);
uint32_t arc4random_uniform(uint32_t upper_bound);

#endif
