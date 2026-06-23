#include <spawn.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <string_view>
#include <span>

import nt;
import print;
import unique_handle;
import exec;

#undef errno
import errno;

using namespace std;

extern "C"
int posix_spawn(pid_t* pid, const char* path,
                const posix_spawn_file_actions_t* /*file_actions*/,
                const posix_spawnattr_t* /*attrp*/,
                char* const* argv, char* const* envp) {
    unique_handle proc, thread;
    nt_status status;
    CLIENT_ID client_id;

    // FIXME - file_actions
    // FIXME - attrp

    if (!path || path[0] == 0)
        return EINVAL;

    auto ret = do_exec(path, argv, envp, proc, thread, client_id);
    if (ret)
        return ret;

    // start thread

    status = NtResumeThread(thread.get(), nullptr);
    if (!success(status)) {
        NtTerminateProcess(proc.get(), status);
        return nt_status_to_errno(status);
    }

    // FIXME - mark process as child

    if (pid)
        *pid = client_id.UniqueProcess;

    return 0;
}

extern "C"
pid_t waitpid(pid_t pid, int* wstatus, int options) {
    nt_status status;
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    CLIENT_ID client_id;
    LARGE_INTEGER timeout;
    PROCESS_BASIC_INFORMATION pbi;
    unique_handle proc;

    if (pid <= 0) {
        print("FIXME - waitpid {}\n", pid); // FIXME
        *_errno() = EINVAL;
        return -1;
    }

    if (options & WCONTINUED) {
        print("FIXME - waitpid with WCONTINUED\n");
        *_errno() = EINVAL;
        return -1;
    }

    if (options & WUNTRACED) {
        print("FIXME - waitpid with WUNTRACED\n");
        *_errno() = EINVAL;
        return -1;
    }

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);

    client_id.UniqueProcess = (HANDLE)pid;
    client_id.UniqueThread = 0;

    // FIXME - check pid again after wait, in case it has been exec'd

    status = NtOpenProcess(&h, SYNCHRONIZE, &oa, &client_id);
    if (status == nt_status::INVALID_CID) {
        *_errno() = ECHILD;
        return -1;
    } else if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    // FIXME - return ECHILD if not our child

    proc.reset(h);

    timeout.QuadPart = 0;

    while (true) {
        status = NtWaitForSingleObject(proc.get(), true,
                                       options & WNOHANG ? &timeout : nullptr);
        if (status == nt_status::USER_APC)
            continue;

        if (status == nt_status::TIMEOUT)
            return 0;

        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }

        break;
    }

    status = NtQueryInformationProcess(proc.get(), PROCESSINFOCLASS::BasicInformation,
                                       &pbi, sizeof(pbi), nullptr);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    if (wstatus) {
        auto exit_code = (uint8_t)pbi.ExitStatus;

        *wstatus = exit_code << 8;
    }

    // FIXME - remove process from zombies list

    return pid;
}
