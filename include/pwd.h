#pragma once

#include <sys/types.h>
#include <stddef.h>

struct passwd {
    char* pw_name;
    uid_t pw_uid;
    gid_t pw_gid;
    char* pw_dir;
    char* pw_shell;
};

#ifdef __cplusplus
extern "C" {
#endif

int getpwnam_r(const char* name, struct passwd* pwd, char* buf, size_t size,
               struct passwd** result);
int getpwuid_r(uid_t uid, struct passwd* pwd, char* buf, size_t size,
               struct passwd** result);
struct passwd* getpwuid(uid_t uid);
struct passwd* getpwnam(const char* name);

#ifdef __cplusplus
}
#endif
