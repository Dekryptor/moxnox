#include "shell.h"
#include <nt.h>

using namespace std;

extern "C"
void startup() {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;
    char buf[4096];

    // FIXME - directory as parameter
    // FIXME - wildcard as parameter

    bool first = true;

    while (true) {
        auto status = NtQueryDirectoryFile(peb->ProcessParameters->CurrentDirectory.Handle, 0, nullptr, nullptr,
                                           &iosb, buf, sizeof(buf), FILE_INFORMATION_CLASS::DirectoryInformation,
                                           false, nullptr, first);

        if (status == nt_status::NO_MORE_FILES)
            break;
        else if (!success(status)) {
            print("NtQueryDirectoryFile returned "); // FIXME - should be to stderr
            print_hex((uint32_t)status, 8);
            print("\n");
            NtTerminateThread(NtCurrentThread(), status);
            return;
        }

        auto& fdi = *(FILE_DIRECTORY_INFORMATION*)buf;
        auto u16sv = u16string_view(fdi.FileName, fdi.FileNameLength / sizeof(char16_t));
        auto len = utf16_to_utf8_len(u16sv);
        char s[255];

        utf16_to_utf8(s, u16sv);

        // FIXME - print times and file sizes (and attributes?)

        if (fdi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            print("\e[01;34m");

        print(string_view(s, len));

        if (fdi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            print("\e[0m");

        print("\n");

        first = false;
    }

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
