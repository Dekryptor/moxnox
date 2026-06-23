#include <spawn.h>

import print;

#undef errno
import errno;

extern "C"
int posix_spawn(pid_t* /*pid*/, const char* /*path*/,
                const posix_spawn_file_actions_t* /*file_actions*/,
                const posix_spawnattr_t* /*attrp*/,
                char* const* /*argv*/, char* const* /*envp*/) {
    print("FIXME - posix_spawn\n");

    // FIXME

    return EINVAL;
}
