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

static void mkdir(u16string_view fn) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE file;
    IO_STATUS_BLOCK iosb;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&file, 0, &oa, &iosb, nullptr, FILE_ATTRIBUTE_DIRECTORY,
                          0, FILE_CREATE, FILE_DIRECTORY_FILE, nullptr, 0);

    if (status == nt_status::OBJECT_NAME_COLLISION) {
        print_stderr("mkdir: {}: File exists\n", fn);
        return;
    } else if (status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print_stderr("mkdir: {}: Path not found\n", fn);
        return;
    } else if (status == nt_status::OBJECT_NAME_INVALID) {
        print_stderr("mkdir: {}: Invalid name\n", fn);
        return;
    } else if (!success(status)) {
        print_stderr("mkdir: {}: NtCreateFile returned {}\n", fn, status);
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

        mkdir(u16sv);

        free(u16);
    }

    // FIXME - return value should be different if anything failed

    return 0;
}
