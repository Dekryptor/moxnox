// SPDX-License-Identifier: GPL-2.0-only
#include <string_view>
#include <span>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

import nt;
import ntdll;
import print;
import unicode;

using namespace std;

static void rmdir(u16string_view fn) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE file;
    IO_STATUS_BLOCK iosb;
    FILE_DISPOSITION_INFORMATION fdi;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    // FIXME - or FILE_DELETE_ON_CLOSE?
    status = NtCreateFile(&file, DELETE, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);

    if (status == nt_status::OBJECT_NAME_NOT_FOUND) {
        print_stderr("rmdir: failed to remove {}: No such file or directory\n", fn);
        return;
    } else if (status == nt_status::NOT_A_DIRECTORY) {
        print_stderr("rmdir: failed to remove {}: Not a directory\n", fn);
        return;
    } else if (!success(status)) {
        print_stderr("rmdir: failed to remove {}: NtCreateFile returned {}\n", fn, status);
        return;
    }

    // FIXME - should we be asking for POSIX-style deletes?

    fdi.DeleteFile = true;

    status = NtSetInformationFile(file, &iosb, &fdi, sizeof(fdi),
                                  FILE_INFORMATION_CLASS::DispositionInformation);
    if (status == nt_status::DIRECTORY_NOT_EMPTY) {
        print_stderr("rmdir: failed to remove {}: Directory not empty\n", fn);
        NtClose(file);
        return;
    } else if (!success(status)) {
        print_stderr("rmdir: failed to remove {}: NtSetInformationFile returned {}\n", fn, status);
        NtClose(file);
        return;
    }

    NtClose(file);
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        auto sv = string_view(argv[i]);

        auto u16len = utf8_to_utf16_len(sv);
        auto u16 = (char16_t*)malloc(u16len * sizeof(char16_t));
        if (!u16) {
            print("malloc failed\n");
            return 1;
        }

        utf8_to_utf16(span(u16, u16len), sv);

        auto u16sv = u16string_view(u16, u16len);

        rmdir(u16sv);

        free(u16);
    }

    // FIXME - return value should be different if anything failed

    return 0;
}
