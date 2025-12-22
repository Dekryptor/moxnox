#include <string_view>
#include <span>
#include <string.h>
#include <stdint.h>

import nt;
import ntdll;
import shelllib;
import print;
import cmdline;

using namespace std;

static void rm_file(u16string_view fn) {
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
                          FILE_OPEN, FILE_NON_DIRECTORY_FILE, nullptr, 0);

    if (status == nt_status::OBJECT_NAME_NOT_FOUND) {
        print("rm: cannot remove {}: No such file or directory\n", fn); // FIXME - stderr
        return;
    } else if (status == nt_status::FILE_IS_A_DIRECTORY) {
        print("rm: cannot remove {}: Is a directory\n", fn); // FIXME - stderr
        return;
    } else if (!success(status)) {
        print("rm: cannot remove {}: NtCreateFile returned {}\n", fn, status); // FIXME - stderr
        return;
    }

    // FIXME - should we be asking for POSIX-style deletes?

    fdi.DeleteFile = true;

    status = NtSetInformationFile(file, &iosb, &fdi, sizeof(fdi),
                                  FILE_INFORMATION_CLASS::DispositionInformation);
    if (!success(status)) {
        print("rm: cannot remove {}: NtSetInformationFile returned {}\n", fn, status); // FIXME - stderr
        NtClose(file);
        return;
    }

    NtClose(file);
}

#ifdef __x86_64__
#define FORCE_ALIGN_ARG_POINTER __attribute__((force_align_arg_pointer))
#else
#define FORCE_ALIGN_ARG_POINTER
#endif

extern "C" FORCE_ALIGN_ARG_POINTER void startup() {
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
        rm_file(argv[i]);
    }

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    // FIXME - return value should be different if anything failed

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
