#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <memory>
#include <string_view>
#include <span>

import nt;
import ntdll;
import unique_handle;
import unicode;

#undef errno
import errno;

using namespace std;

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

extern "C"
int setpgid(pid_t pid, pid_t pgid) {
    auto& teb = *NtCurrentTeb();
    nt_status status;
    HANDLE proc;
    unique_handle uh;
    uintptr_t pgid2;

    if (pgid < 0) {
        *_errno() = EINVAL;
        return -1;
    }

    if (pid == 0)
        proc = NtCurrentProcess();
    else {
        OBJECT_ATTRIBUTES oa;
        CLIENT_ID client_id;

        memset(&oa, 0, sizeof(oa));
        oa.Length = sizeof(OBJECT_ATTRIBUTES);

        client_id.UniqueProcess = (HANDLE)pid;
        client_id.UniqueThread = 0;

        status = NtOpenProcess(&proc, PROCESS_SET_INFORMATION, &oa,
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

    if (pgid == 0) {
        if (pid == 0)
            pgid2 = teb.ClientId.UniqueProcess;
        else
            pgid2 = (uintptr_t)pid;
    } else
        pgid2 = (uintptr_t)pgid;

    status = NtSetInformationProcess(proc, PROCESS_INFORMATION_CLASS::MoxnoxSetProcessGroup,
                                     &pgid2, sizeof(pgid2));
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
pid_t getpgid(pid_t pid) {
    nt_status status;
    HANDLE proc;
    unique_handle uh;
    uintptr_t ret;
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

    status = NtQueryInformationProcess(proc, PROCESS_INFORMATION_CLASS::MoxnoxGetProcessGroup,
                                       &ret, sizeof(ret), &retlen);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return (pid_t)ret;
}

extern "C"
void* dlopen(const char* file, int flags) {
    nt_status status;
    UNICODE_STRING us;
    void* dll;
    unique_ptr<char16_t[]> u16buf;

    static const int supported_flags = RTLD_LAZY | RTLD_NOW | RTLD_GLOBAL | RTLD_LOCAL;

    // FIXME - if filename contains slash, treat as path (LdrLoadDll should handle this)

    // ignoring RTLD_LAZY and RTLD_NOW

    // FIXME - RTLD_GLOBAL and RTLD_LOCAL?

    if (flags & ~supported_flags) {
        // FIXME - store error
        return nullptr;
    }

    if (!file) {
        // FIXME - should return address of main program
        return nullptr;
    }

    auto sv = string_view(file);

    if (sv.empty()) {
        // FIXME - store error
        return nullptr;
    }

    auto u16len = utf8_to_utf16_len(sv);

    u16buf.reset(new char16_t[u16len]);
    if (!u16buf) {
        // FIXME - store error
        return nullptr;
    }

    utf8_to_utf16(span(u16buf.get(), u16len), sv);

    us.Buffer = u16buf.get();
    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));

    status = LdrLoadDll(nullptr, nullptr, &us, &dll);
    if (!success(status)) {
        // FIXME - store error
        return nullptr;
    }

    return dll;
}
