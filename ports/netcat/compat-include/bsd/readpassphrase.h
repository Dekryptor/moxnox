#ifndef NETCAT_COMPAT_BSD_READPASSPHRASE_H
#define NETCAT_COMPAT_BSD_READPASSPHRASE_H

#include <stddef.h>

#define RPP_ECHO_OFF 0x00
#define RPP_ECHO_ON 0x01
#define RPP_REQUIRE_TTY 0x02
#define RPP_FORCELOWER 0x04
#define RPP_FORCEUPPER 0x08
#define RPP_SEVENBIT 0x10
#define RPP_STDIN 0x20

char* readpassphrase(const char* prompt, char* buf, size_t bufsiz, int flags);

#endif
