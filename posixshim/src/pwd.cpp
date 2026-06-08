#include <pwd.h>
#include <string.h>

#undef errno
import errno;

static char static_buf[1024];

extern "C"
int getpwuid_r(uid_t uid, struct passwd* pwd, char* buf, size_t size,
               struct passwd** result) {
    static const char NAME[] = "root";
    static const char DIR[] = "/";
    static const char SHELL[] = "/bin/shell.exe";

    if (uid != 0) {
        *result = nullptr;
        return 0;
    }

    if (size < sizeof(NAME) + sizeof(DIR) + sizeof(SHELL))
        return ERANGE;

    memcpy(buf, NAME, sizeof(NAME));
    memcpy(buf + sizeof(NAME), DIR, sizeof(DIR));
    memcpy(buf + sizeof(NAME) + sizeof(DIR), SHELL, sizeof(SHELL));

    pwd->pw_name = buf;
    pwd->pw_uid = uid;
    pwd->pw_gid = 0;
    pwd->pw_dir = buf + sizeof(NAME);
    pwd->pw_shell = buf + sizeof(NAME) + sizeof(DIR);

    *result = pwd;

    return 0;
}

extern "C"
struct passwd* getpwuid(uid_t uid) {
    static struct passwd pwd;
    struct passwd* result;

    if (getpwuid_r(uid, &pwd, static_buf, sizeof(static_buf), &result) != 0)
        return nullptr;

    return result;
}

extern "C"
int getpwnam_r(const char* name, struct passwd* pwd, char* buf, size_t size,
               struct passwd** result) {
    if (strcmp(name, "root")) {
        *result = nullptr;
        return 0;
    }

    return getpwuid_r(0, pwd, buf, size, result);
}
