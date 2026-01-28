#include <string_view>
#include <span>
#include <utility>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

import nt;
import ntdll;
import print;
import cmdline;
import unique_handle;

using namespace std;

static nt_status mv(u16string_view src, u16string_view dest) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle file, desth;

    // try to open dest

    us.Length = us.MaximumLength = (uint16_t)(dest.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)dest.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);
    if (success(status))
        desth.reset(h);
    else if (status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print_stderr("mv: cannot move '{}' to '{}': No such file or directory\n",
                     src, dest);
        return status;
    } else if (status != nt_status::OBJECT_NAME_NOT_FOUND && status != nt_status::NOT_A_DIRECTORY) {
        print_stderr("mv: '{}': NtCreateFile returned {}\n", dest, status);
        return status;
    }

    // open src

    us.Length = us.MaximumLength = (uint16_t)(src.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)src.data();

    status = NtCreateFile(&h, DELETE, &oa, &iosb, nullptr, 0, 0, FILE_OPEN,
                          0, nullptr, 0);
    if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
        status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print_stderr("mv: '{}': No such file or directory\n", src);
        return status;
    } else if (!success(status)) {
        print_stderr("mv: '{}': NtCreateFile returned {}\n", src, status);
        return status;
    }

    file.reset(h);

    // rename

    u16string_view fn;

    if (desth) {
        if (auto slash = src.find(u'/'); slash != u16string_view::npos)
            fn = u16string_view(src.data(), slash);
        else
            fn = src;

        oa.RootDirectory = desth.get();
    } else {
        fn = dest;
    }

    size_t frilen = offsetof(FILE_RENAME_INFORMATION, FileName) + (fn.size() * sizeof(char16_t));

    auto fri = (FILE_RENAME_INFORMATION*)malloc(frilen);
    if (!fri) {
        print_stderr("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    fri->ReplaceIfExists = true; // FIXME? --interactive option
    fri->RootDirectory = desth ? desth.get() : peb->ProcessParameters->CurrentDirectory.Handle;
    fri->FileNameLength = (uint32_t)(fn.size() * sizeof(char16_t));
    memcpy(fri->FileName, fn.data(), fn.size() * sizeof(char16_t));

    status = NtSetInformationFile(file.get(), &iosb, fri, (uint32_t)frilen,
                                  FILE_INFORMATION_CLASS::RenameInformation);
    if (!success(status)) {
        print("mv: NtSetInformationFile returned {}\n", status);
        return status;
    }

    return nt_status::SUCCESS;
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

    if (!argv.empty()) // skip program name
        argv = span(argv.data() + 1, argv.size() - 1);

    if (argv.empty()) {
        print_stderr("mv: missing operand\n");
        NtTerminateThread(NtCurrentThread(), nt_status::INVALID_PARAMETER);
        unreachable();
    }

    if (argv.size() < 2) {
        print_stderr("mv: missing destination file operand after '{}'\n", argv[0]);
        NtTerminateThread(NtCurrentThread(), nt_status::INVALID_PARAMETER);
        unreachable();
    }

    if (argv.size() > 2) {
        print_stderr("FIXME - multiple source operands\n"); // FIXME
        NtTerminateThread(NtCurrentThread(), nt_status::NOT_IMPLEMENTED);
        unreachable();
    }

    auto ret = mv(argv[0], argv[1]);

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    NtTerminateThread(NtCurrentThread(), ret);
}
