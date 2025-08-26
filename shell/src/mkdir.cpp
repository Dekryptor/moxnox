#include <nt.h>
#include <ntdll.h>
#include <string_view>
#include <span>
#include <string.h>

import shelllib;
import print;
import cmdline;

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
        print("mkdir: {}: File exists\n", fn); // FIXME - stderr
        return;
    } else if (status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print("mkdir: {}:: Path not found\n", fn); // FIXME - stderr
        return;
    } else if (status == nt_status::OBJECT_NAME_INVALID) {
        print("mkdir: {}: Invalid name\n", fn); // FIXME - stderr
        return;
    } else if (!success(status)) {
        print("mkdir: {}: NtCreateFile returned {}\n", fn, status); // FIXME - stderr
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

    if (argv.size() < 2) {
        print("mkdir: missing operand\n"); // FIXME - stderr
        NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
        return;
    }

    for (size_t i = 1; i < argv.size(); i++) {
        mkdir(argv[i]);
    }

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    // FIXME - return value should be different if anything failed

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
