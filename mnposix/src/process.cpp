#include <unistd.h>
#include <string.h>
#include <errno.h>

import nt;
import unique_handle;

#undef errno
import errno;

extern "C"
pid_t getsid(pid_t pid) {
    nt_status status;
    HANDLE proc;
    unique_handle uh;
    uintptr_t sess;
    uint32_t retlen;

    if (pid == 0)
        proc = NtCurrentProcess();
    else {
        OBJECT_ATTRIBUTES oa;
        CLIENT_ID client_id;

        memset(&oa, 0, sizeof(oa));
        oa.Length = sizeof(OBJECT_ATTRIBUTES);

        client_id.UniqueProcess = (HANDLE)pid;
        client_id.UniqueThread = 0;

        status = NtOpenProcess(&proc, PROCESS_QUERY_INFORMATION, &oa,
                               &client_id);
        if (status == nt_status::INVALID_CID) {
            *_errno() = ESRCH;
            return -1;
        } else if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }

        uh.reset(proc);
    }

    status = NtQueryInformationProcess(proc, PROCESS_INFORMATION_CLASS::MoxnoxGetSessionId,
                                       &sess, sizeof(sess), &retlen);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return (pid_t)sess;
}

extern "C"
pid_t setsid() {
    auto& teb = *NtCurrentTeb();

    auto status = NtSetInformationProcess(NtCurrentProcess(),
                                          PROCESS_INFORMATION_CLASS::MoxnoxNewSession,
                                          nullptr, 0);
    if (status == nt_status::INVALID_PARAMETER) {
        *_errno() = EPERM;
        return -1;
    } else if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return teb.ClientId.UniqueProcess;
}
