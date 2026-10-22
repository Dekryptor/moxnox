// SPDX-License-Identifier: LGPL-2.1-or-later
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#undef errno
import errno;

static char static_buf[1024];
static char static_buf2[1024];

static size_t user_index = 0;
static struct passwd pwent;
static char pwent_buf[1024];

extern "C"
int getpwuid_r(uid_t uid, struct passwd* pwd, char* buf, size_t size,
               struct passwd** result) {
    static const char NAME[] = "root";
    static const char DIR[] = "/root";
    static const char SHELL[] = "/bin/bash.exe";

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

extern "C"
struct passwd* getpwnam(const char* name) {
    static struct passwd pwd;
    struct passwd* result;

    if (getpwnam_r(name, &pwd, static_buf2, sizeof(static_buf2), &result) != 0)
        return nullptr;

    return result;
}

extern "C"
int getgrgid_r(gid_t gid, struct group* grp, char* buf, size_t size,
               struct group** result) {
    static const char NAME[] = "root";
    static const char MEMBER[] = "root";

    if (gid != 0) {
        *result = nullptr;
        return 0;
    }

    // FIXME - gr_mem array ought to be aligned

    if (size < sizeof(NAME) + sizeof(MEMBER) + (2 * sizeof(char*)))
        return ERANGE;

    memcpy(buf, NAME, sizeof(NAME));
    memcpy(buf + sizeof(NAME), MEMBER, sizeof(MEMBER));

    grp->gr_name = buf;
    grp->gr_gid = gid;
    grp->gr_mem = (char**)(buf + sizeof(NAME) + sizeof(MEMBER));

    grp->gr_mem[0] = buf + sizeof(NAME);
    grp->gr_mem[1] = nullptr;

    *result = grp;

    return 0;
}

extern "C"
int getgrnam_r(const char* name, struct group* grp, char* buf, size_t size,
               struct group** result) {
    if (strcmp(name, "root")) {
        *result = nullptr;
        return 0;
    }

    return getgrgid_r(0, grp, buf, size, result);
}

extern "C"
struct group* getgrgid(gid_t gid) {
    static struct group grp;
    static char buf[1024];
    struct group* result;

    if (getgrgid_r(gid, &grp, buf, sizeof(buf), &result) != 0)
        return nullptr;

    return result;
}

extern "C"
struct group* getgrnam(const char* name) {
    static struct group grp;
    static char buf[1024];
    struct group* result;

    if (getgrnam_r(name, &grp, buf, sizeof(buf), &result) != 0)
        return nullptr;

    return result;
}

extern "C"
int getlogin_r(char* name, size_t namesize) {
    char buf[256];
    passwd pwd;
    passwd* res;

    auto ret = getpwuid_r(getuid(), &pwd, buf, sizeof(buf), &res);
    if (ret != 0)
        return ret;

    if (!res)
        return ENXIO;

    if (strlen(res->pw_name) >= namesize)
        return ERANGE;

    strcpy(name, res->pw_name);

    return 0;
}

extern "C"
char* getlogin(void) {
    static char buf[256];

    if (getlogin_r(buf, sizeof(buf)) != 0)
        return nullptr;

    return buf;
}

extern "C"
void endpwent() {
    // nop

    user_index = 0;
}

extern "C"
struct passwd* getpwent(void) {
    struct passwd* res;

    // FIXME

    if (user_index > 0)
        return nullptr;

    auto ret = getpwuid_r(0, &pwent, pwent_buf, sizeof(pwent_buf), &res);
    if (ret) {
        *_errno() = ret;
        return nullptr;
    }

    user_index++;

    return res;
}

extern "C"
void setpwent() {
    user_index = 0;
}
