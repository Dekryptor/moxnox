#include <string_view>
#include <span>
#include <string.h>
#include <stdint.h>

import nt;
import ntdll;
import print;
import cmdline;

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

extern "C" void startup() {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;

    auto cmdline = u16string_view(peb->ProcessParameters->CommandLine.Buffer,
                                  peb->ProcessParameters->CommandLine.Length / sizeof(char16_t));

    auto argc = count_cmdline_args(cmdline);
    span<u16string_view> argv;
    u16string_view* buf = nullptr;

    if (argc != 0) {
        buf = (decltype(buf))RtlAllocateHeap(GetProcessHeap(), 0, argc * sizeof(*buf));
        if (!buf)
            NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);

        argv = span(buf, argc);
    }

    parse_cmdline(cmdline, argv);

    if (argv.size() < 2) {
        print_stderr("touch: missing operand\n");
        NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
        return;
    }

    for (size_t i = 1; i < argv.size(); i++) {
        touch(argv[i]);
    }

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    // FIXME - return value should be different if anything failed

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
