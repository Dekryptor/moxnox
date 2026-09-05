#include <unistd.h>
#include <grp.h>

import ucrtbase;

#undef errno
import errno;

static size_t group_index = 0;
static struct group grent;
static const char* grent_mem[] = { "root", nullptr };

extern "C"
uid_t getuid() {
    // FIXME

    return 0;
}

extern "C"
gid_t getgid() {
    // FIXME

    return 0;
}

extern "C"
uid_t geteuid() {
    // FIXME

    return 0;
}

extern "C"
gid_t getegid() {
    // FIXME

    return 0;
}

extern "C"
int getresuid(uid_t* ruid, uid_t* euid, uid_t* suid) {
    // FIXME

    *ruid = 0;
    *euid = 0;
    *suid = 0;

    return 0;
}

extern "C"
int getresgid(gid_t* rgid, gid_t* egid, gid_t* sgid) {
    // FIXME

    *rgid = 0;
    *egid = 0;
    *sgid = 0;

    return 0;
}

extern "C"
void endgrent() {
    // nop

    group_index = 0;
}

extern "C"
struct group* getgrent() {
    // FIXME

    if (group_index > 0)
        return nullptr;

    grent.gr_name = (char*)"root";
    grent.gr_gid = 0;
    grent.gr_mem = (char**)grent_mem;

    group_index++;

    return &grent;
}

extern "C"
void setgrent() {
    group_index = 0;
}

extern "C"
int setuid(uid_t uid) {
    // FIXME

    if (uid != 0) {
        *_errno() = EINVAL;
        return -1;
    }

    return 0;
}

extern "C"
int setgid(gid_t gid) {
    // FIXME

    if (gid != 0) {
        *_errno() = EINVAL;
        return -1;
    }

    return 0;
}

extern "C"
int seteuid(uid_t uid) {
    // FIXME

    if (uid != 0) {
        *_errno() = EINVAL;
        return -1;
    }

    return 0;
}

extern "C"
int setegid(gid_t gid) {
    // FIXME

    if (gid != 0) {
        *_errno() = EINVAL;
        return -1;
    }

    return 0;
}

extern "C"
int setresuid(uid_t ruid, uid_t euid, uid_t suid) {
    // FIXME

    if (ruid != 0 && ruid != (uid_t)-1) {
        *_errno() = EINVAL;
        return -1;
    }

    if (euid != 0 && euid != (uid_t)-1) {
        *_errno() = EINVAL;
        return -1;
    }

    if (suid != 0 && suid != (uid_t)-1) {
        *_errno() = EINVAL;
        return -1;
    }

    return 0;
}

extern "C"
int setresgid(gid_t rgid, gid_t egid, gid_t sgid) {
    // FIXME

    if (rgid != 0 && rgid != (gid_t)-1) {
        *_errno() = EINVAL;
        return -1;
    }

    if (egid != 0 && egid != (gid_t)-1) {
        *_errno() = EINVAL;
        return -1;
    }

    if (sgid != 0 && sgid != (gid_t)-1) {
        *_errno() = EINVAL;
        return -1;
    }

    return 0;
}

extern "C"
int getgroups(int gidsetsize, gid_t* /*grouplist*/) {
    if (gidsetsize < 0) {
        *_errno() = EINVAL;
        return -1;
    }

    // FIXME

    return 0;
}
