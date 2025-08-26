#include <nt.h>
#include <ntdll.h>
#include <string_view>
#include <span>
#include <string.h>

import shelllib;
import print;
import cmdline;

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
        print("rmdir: failed to remove {}: No such file or directory\n", fn); // FIXME - stderr
        return;
    } else if (status == nt_status::NOT_A_DIRECTORY) {
        print("rmdir: failed to remove {}: Not a directory\n", fn); // FIXME - stderr
        return;
    } else if (!success(status)) {
        print("rmdir: failed to remove {}: NtCreateFile returned {}\n", fn, status); // FIXME - stderr
        return;
    }

    // FIXME - should we be asking for POSIX-style deletes?

    fdi.DeleteFile = true;

    status = NtSetInformationFile(file, &iosb, &fdi, sizeof(fdi),
                                  FILE_INFORMATION_CLASS::DispositionInformation);
    if (status == nt_status::DIRECTORY_NOT_EMPTY) {
        print("rmdir: failed to remove {}: Directory not empty\n", fn); // FIXME - stderr
        NtClose(file);
        return;
    } else if (!success(status)) {
        print("rmdir: failed to remove {}: NtSetInformationFile returned {}\n", fn, status); // FIXME - stderr
        NtClose(file);
        return;
    }

    NtClose(file);
}

extern "C"
__attribute__((force_align_arg_pointer)) void startup() {
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

    for (size_t i = 1; i < argv.size(); i++) {
        rmdir(argv[i]);
    }

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    // FIXME - return value should be different if anything failed

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
