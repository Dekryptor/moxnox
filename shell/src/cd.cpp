module;

#include <string_view>
#include <span>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

export module cd;

import nt;
import print;
import unicode;

using namespace std;

export void cd(string_view param) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    HANDLE h;
    IO_STATUS_BLOCK iosb;
    nt_status status;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    if (param.empty()) {
        print("FIXME - cd with no parameter\n"); // FIXME
        return;
    }

    auto len = utf8_to_utf16_len(param);

    auto buf = (char16_t*)malloc(len * sizeof(char16_t));
    if (!buf) {
        print("malloc failed\n");
        return;
    }

    utf8_to_utf16(span(buf, len), param);

    us.Length = us.MaximumLength = (uint16_t)(len * sizeof(char16_t));
    us.Buffer = (char16_t*)buf;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_TRAVERSE | SYNCHRONIZE, &oa, &iosb, nullptr, 0,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_ALERT,
                          nullptr, 0);
    free(buf);

    if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
        status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print("cd: {}: No such file or directory\n", param); // FIXME - stderr
        return;
    } else if (status == nt_status::NOT_A_DIRECTORY) {
        print("cd: {}: Not a directory\n", param); // FIXME - stderr
        return;
    } else if (!success(status)) {
        print("cd: {}: NtCreateFile returned {}\n", param, status); // FIXME - stderr
        return;
    }

    // FIXME - do while holding PEB lock

    if (peb->ProcessParameters->CurrentDirectory.Handle != 0)
        NtClose(peb->ProcessParameters->CurrentDirectory.Handle);

    peb->ProcessParameters->CurrentDirectory.Handle = h;

    // FIXME - set peb->ProcessParameters->CurrentDirectory.DosPath
}
