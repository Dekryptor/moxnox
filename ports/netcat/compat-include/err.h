#ifndef NETCAT_COMPAT_ERR_H
#define NETCAT_COMPAT_ERR_H

void err(int, const char*, ...) __attribute__((format(printf, 2, 3), noreturn));
void errx(int, const char*, ...) __attribute__((format(printf, 2, 3), noreturn));
void warn(const char*, ...) __attribute__((format(printf, 1, 2)));
void warnx(const char*, ...) __attribute__((format(printf, 1, 2)));

#endif
