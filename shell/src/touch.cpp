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

static void touch(u16string_view fn) {
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

    status = NtCreateFile(&file, FILE_WRITE_ATTRIBUTES, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN_IF,
                          0, nullptr, 0);

    if (!success(status)) {
        print_stderr("touch: {}: NtCreateFile returned {}\n", fn, status);
        return;
    }

    if (iosb.Information != FILE_CREATED) {
        FILE_BASIC_INFORMATION fbi;
        LARGE_INTEGER now;

        status = NtQuerySystemTime(&now);
        if (!success(status)) {
            print_stderr("touch: {}: NtQuerySystemTime returned {}\n", fn, status);
            NtClose(file);
            return;
        }

        memset(&fbi, 0, sizeof(fbi));

        fbi.LastAccessTime.QuadPart = now.QuadPart;
        fbi.LastWriteTime.QuadPart = now.QuadPart;
        fbi.ChangeTime.QuadPart = now.QuadPart;

        status = NtSetInformationFile(file, &iosb, &fbi, sizeof(fbi), FILE_INFORMATION_CLASS::BasicInformation);

        if (!success(status)) {
            print_stderr("touch: {}: NtSetInformationFile returned {}\n", fn, status);
            NtClose(file);
            return;
        }
    }

    NtClose(file);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_stderr("touch: missing operand\n");
        return 1;
    }

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

        touch(u16sv);

        free(u16);
    }

    // FIXME - return value should be different if anything failed

    return 0;
}
