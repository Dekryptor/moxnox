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

int getpwnam_r(const char* name, struct passwd* pwd, char* buf, size_t size,
               struct passwd** result);
int getpwuid_r(uid_t uid, struct passwd* pwd, char* buf, size_t size,
               struct passwd** result);
