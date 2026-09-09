#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <err.h>

static void vwarn_common(const char* fmt, va_list args, bool with_errno) {
    int saved_errno = errno;

    fprintf(stderr, "%s: ", getprogname());

    if (fmt) {
        vfprintf(stderr, fmt, args);

        if (with_errno)
            fputs(": ", stderr);
    }

    if (with_errno)
        fputs(strerror(saved_errno), stderr);

    fputc('\n', stderr);
}

void err(int eval, const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vwarn_common(fmt, args, true);
    va_end(args);

    exit(eval);
}

void errx(int eval, const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vwarn_common(fmt, args, false);
    va_end(args);

    exit(eval);
}

void warn(const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vwarn_common(fmt, args, true);
    va_end(args);
}

void warnx(const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vwarn_common(fmt, args, false);
    va_end(args);
}

int _ssh_compat_getentropy(void* s, size_t len) {
    return getentropy(s, len);
}
