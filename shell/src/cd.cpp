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
import env;

using namespace std;

static void get_dos_path(HANDLE h, UNICODE_STRING& us) {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    FILE_NAME_INFORMATION fni;

    status = NtQueryInformationFile(h, &iosb, &fni, sizeof(fni),
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status) && status != nt_status::BUFFER_OVERFLOW) {
        print_stderr("NtQueryInformationFile returned {}\n", status);
        return;
    }

    size_t len = offsetof(FILE_NAME_INFORMATION, FileName) + fni.FileNameLength;

    auto fni2 = (FILE_NAME_INFORMATION*)malloc(len);
    if (!fni2) {
        print_stderr("malloc failed\n");
        return;
    }

    status = NtQueryInformationFile(h, &iosb, fni2, (uint32_t)len,
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status)) {
        print_stderr("NtQueryInformationFile returned {}\n", status);
        free(fni2);
        return;
    }

    auto sv = u16string_view(fni2->FileName, fni2->FileNameLength / sizeof(char16_t));

    static const u16string_view prefix = u"C:";

    us.Length = min(us.MaximumLength, (uint16_t)((sv.size() + prefix.size()) * sizeof(char16_t)));
    memcpy(us.Buffer, prefix.data(), prefix.size() * sizeof(char16_t));
    memcpy(us.Buffer + prefix.size(), sv.data(), us.Length - prefix.size());

    for (size_t i = prefix.size(); i < us.Length; i++) {
        if (us.Buffer[i] == u'/')
            us.Buffer[i] = u'\\';
    }

    free(fni2);
}

export void cd(string_view param) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    HANDLE h;
    IO_STATUS_BLOCK iosb;
    nt_status status;
    char16_t* buf = nullptr;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    if (param.empty()) {
        auto env_home = get_env(u"HOME");

        if (env_home.empty()) {
            static const u16string_view def = u"/";

            us.Length = us.MaximumLength = (uint16_t)(def.size() * sizeof(char16_t));
            us.Buffer = (char16_t*)def.data();
        } else {
            us.Length = us.MaximumLength = (uint16_t)(env_home.size() * sizeof(char16_t));
            us.Buffer = (char16_t*)env_home.data();
        }
    } else {
        auto len = utf8_to_utf16_len(param);

        auto buf = (char16_t*)malloc(len * sizeof(char16_t));
        if (!buf) {
            print_stderr("malloc failed\n");
            return;
        }

        utf8_to_utf16(span(buf, len), param);

        us.Length = us.MaximumLength = (uint16_t)(len * sizeof(char16_t));
        us.Buffer = (char16_t*)buf;
    }

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_TRAVERSE | SYNCHRONIZE, &oa, &iosb, nullptr, 0,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_ALERT,
                          nullptr, 0);
    if (buf)
        free(buf);

    if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
        status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print_stderr("cd: {}: No such file or directory\n", param);
        return;
    } else if (status == nt_status::NOT_A_DIRECTORY) {
        print_stderr("cd: {}: Not a directory\n", param);
        return;
    } else if (!success(status)) {
        print_stderr("cd: {}: NtCreateFile returned {}\n", param, status);
        return;
    }

    // FIXME - do while holding PEB lock

    if (peb->ProcessParameters->CurrentDirectory.Handle != 0)
        NtClose(peb->ProcessParameters->CurrentDirectory.Handle);

    peb->ProcessParameters->CurrentDirectory.Handle = h;

    get_dos_path(h, peb->ProcessParameters->CurrentDirectory.DosPath);
}
