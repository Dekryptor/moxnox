#pragma once

#include <stdint.h>
#include <inttypes.h>

/*
 * Pull in the (GEN-patched) git-compat-util.h first so its include guard
 * suppresses the unpatched repo copy that every source quote-includes. This
 * transitively brings in the patched compat/posix.h too.
 */
#include "git-compat-util.h"

#include <pwd.h>
#include <sys/wait.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <netinet/in.h>
#include <termios.h>
#include <sys/select.h>

#define ITIMER_REAL  0

int getpagesize(void);

/*
 * git's POSIX locate_in_PATH (run-command.c) searches for the bare command
 * name, but Moxnox executables carry a .exe suffix, so it never finds e.g.
 * git-remote-https.exe. Override it through git's #ifndef locate_in_PATH hook
 * with a .exe-aware version (see moxnox-compat.c).
 */
#define locate_in_PATH moxnox_locate_in_PATH
char *moxnox_locate_in_PATH(const char *file);
