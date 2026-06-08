#include <pwd.h>
#include <string.h>

#undef errno
import errno;

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
