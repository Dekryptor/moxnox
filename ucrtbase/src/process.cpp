#include <stdlib.h>
#include <string.h>
#include <string_view>
#include <span>

import nt;
import kernelbase;
import unique_handle;
import unicode;

#undef errno
import errno;

using namespace std;

extern "C" int* __errno();

extern "C"
int _getpid() {
    PROCESS_BASIC_INFORMATION pbi;

    auto status = NtQueryInformationProcess(NtCurrentProcess(),
                                            PROCESSINFOCLASS::BasicInformation,
                                            &pbi, sizeof(pbi), nullptr);
    if (!success(status))
        return 0;

    return (int)pbi.UniqueProcessId;
}

extern "C"
int system(const char* command) {
    string_view cmd(command);
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    PROCESS_BASIC_INFORMATION pbi;
    nt_status status;

    static const u16string_view DASH_C = u" -c ";

    auto shell = getenv("SHELL");

    if (!command) {
        if (!shell || !shell[0])
            return 0;

        // FIXME - should check whether shell actually exists?

        return 1;
    }

    if (!shell || !shell[0]) {
        *__errno() = ENOENT;
        return -1;
    }

    if (!command[0]) {
        *__errno() = EINVAL;
        return -1;
    }

    auto shellsv = string_view(shell);
    auto u16shelllen = utf8_to_utf16_len(shellsv);
    auto u16cmdlen = utf8_to_utf16_len(cmd);

    auto cmdlinelen = u16shelllen + DASH_C.size() + u16cmdlen + 1;
    auto cmdline = (char16_t*)malloc(cmdlinelen * sizeof(char16_t));
    if (!cmdline) {
        *__errno() = ENOMEM;
        return -1;
    }

    auto u16shell = (char16_t*)malloc((u16shelllen + 1) * sizeof(char16_t));
    if (!u16shell) {
        *__errno() = ENOMEM;
        free(cmdline);
        return -1;
    }

    utf8_to_utf16(span(u16shell, u16shelllen), shellsv);
    u16shell[u16shelllen] = 0;

    memcpy(cmdline, u16shell, u16shelllen * sizeof(char16_t));
    memcpy(cmdline + u16shelllen, DASH_C.data(), DASH_C.size() * sizeof(char16_t));
    utf8_to_utf16(span(cmdline + u16shelllen + DASH_C.size(), u16cmdlen), cmd);
    cmdline[u16shelllen + DASH_C.size() + u16cmdlen] = 0;

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    if (!CreateProcessW(u16shell, cmdline, nullptr, nullptr, false, 0, nullptr,
                        nullptr, &si, &pi)) {
        free(u16shell);
        free(cmdline);

        // FIXME - errno?
        *__errno() = EINVAL;
        return -1;
    }

    free(u16shell);
    free(cmdline);

    NtClose(pi.hThread);

    unique_handle proc{pi.hProcess};

    while (true) {
        status = NtWaitForSingleObject(proc.get(), true, nullptr);
        if (!success(status)) {
            *__errno() = nt_status_to_errno(status);
            return -1;
        }

        if (status == nt_status::SUCCESS)
            break;
    }

    status = NtQueryInformationProcess(proc.get(), PROCESSINFOCLASS::BasicInformation,
                                       &pbi, sizeof(pbi), nullptr);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    return (int)pbi.ExitStatus;
}
