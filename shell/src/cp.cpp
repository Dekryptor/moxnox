#include <string_view>
#include <span>
#include <utility>
#include <algorithm>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

import nt;
import ntdll;
import print;
import cmdline;
import unique_handle;

using namespace std;

static nt_status cp(u16string_view src, u16string_view dest) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    FILE_STANDARD_INFORMATION fsi;
    unique_handle srch, desth, destdir;

    static const size_t COPY_SIZE = 1048576;

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
        destdir.reset(h);
    else if (status != nt_status::OBJECT_PATH_NOT_FOUND &&
             status != nt_status::OBJECT_NAME_NOT_FOUND &&
             status != nt_status::NOT_A_DIRECTORY) {
        print_stderr("cp: '{}': NtCreateFile returned {}\n", dest, status);
        return status;
    }

    // open src

    us.Length = us.MaximumLength = (uint16_t)(src.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)src.data();

    status = NtCreateFile(&h, FILE_READ_DATA | SYNCHRONIZE, &oa, &iosb, nullptr,
                          0, 0, FILE_OPEN,
                          FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_ALERT,
                          nullptr, 0);
    if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
        status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print_stderr("cp: '{}': No such file or directory\n", src);
        return status;
    } else if (status == nt_status::FILE_IS_A_DIRECTORY) {
        print_stderr("cp: -r not specified; omitting directory '{}'\n", src); // FIXME
        return status;
    } else if (!success(status)) {
        print_stderr("cp: '{}': NtCreateFile returned {}\n", src, status);
        return status;
    }

    srch.reset(h);

    // get src file size

    status = NtQueryInformationFile(srch.get(), &iosb, &fsi, sizeof(fsi),
                                    FILE_INFORMATION_CLASS::StandardInformation);
    if (!success(status)) {
        print_stderr("cp: '{}': NtQueryInformationFile returned {}\n", src, status);
        return status;
    }

    // open dst

    u16string_view fn;

    if (destdir) {
        if (auto slash = src.find_last_of(u'/'); slash != u16string_view::npos)
            fn = u16string_view(src.data() + slash + 1, src.size() - slash - 1);
        else
            fn = src;

        oa.RootDirectory = destdir.get();
    } else
        fn = dest;

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

    // FIXME - show error if src and dst are same file (open dst first, and compare inode numbers and device?)

    // FIXME - --interactive option
    // FIXME - preallocate size
    // FIXME - check src is file
    // FIXME - copying things other than files (symlinks, Linux block or char devs, etc.)

    status = NtCreateFile(&h, FILE_WRITE_DATA | SYNCHRONIZE | DELETE, &oa, &iosb,
                          nullptr, 0, 0, FILE_OVERWRITE_IF,
                          FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_ALERT,
                          nullptr, 0);

    if (success(status))
        desth.reset(h);
    else if (status == nt_status::OBJECT_PATH_NOT_FOUND ||
             status == nt_status::OBJECT_NAME_NOT_FOUND) {
        if (destdir)
            print_stderr("cp: cannot create regular file '{}/{}': No such file or directory\n", dest, fn);
        else
            print_stderr("cp: cannot create regular file '{}': No such file or directory\n", dest);
        return status;
    } else {
        if (destdir)
            print_stderr("cp: cannot create regular file '{}/fn': NtCreateFile returned {}\n", dest, fn, status);
        else
            print_stderr("cp: cannot create regular file '{}': NtCreateFile returned {}\n", dest, status);
        return status;
    }

    // FIXME - also copy attributes, xattrs, SD, etc.

    // FIXME - copy by mmap instead?

    auto left = (uint64_t)fsi.EndOfFile.QuadPart;

    if (left == 0)
        return nt_status::SUCCESS;

    auto bufsize = max(COPY_SIZE, left);

    auto buf = malloc(bufsize);
    if (!buf) {
        print_stderr("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    while (left > 0) {
        uint64_t read_bytes;

        status = NtReadFile(srch.get(), 0, nullptr, nullptr, &iosb,
                            buf, (uint32_t)min(left, bufsize), nullptr, nullptr);

        if (status == nt_status::END_OF_FILE)
            break;

        if (!success(status)) {
            print_stderr("cp: '{}': NtReadFile returned {}\n", src, status);
            break;
        }

        read_bytes = iosb.Information;

        if (read_bytes == 0)
            continue;

        span sp((char*)buf, read_bytes);

        do {
            status = NtWriteFile(desth.get(), 0, nullptr, nullptr, &iosb,
                                 sp.data(), (uint32_t)sp.size(), nullptr,
                                 nullptr);
            if (!success(status)) {
                if (destdir)
                    print_stderr("cp: '{}/{}': NtWriteFile returned {}\n", dest, fn, status);
                else
                    print_stderr("cp: '{}': NtWriteFile returned {}\n", dest, status);
                break;
            }

            if (iosb.Information == sp.size())
                break;

            sp = span(sp.data() + iosb.Information, sp.size() - iosb.Information);
        } while (true);

        if (!success(status))
            break;

        left -= read_bytes;
    }

    free(buf);

    if (status == nt_status::END_OF_FILE)
        status = nt_status::SUCCESS;

    if (!success(status)) {
        FILE_DISPOSITION_INFORMATION fdi;

        fdi.DeleteFile = true;

        auto status2 = NtSetInformationFile(desth.get(), &iosb, &fdi, sizeof(fdi),
                                            FILE_INFORMATION_CLASS::DispositionInformation);
        if (!success(status2))
            print_stderr("cp: '{}': NtSetInformationFile returned {} when trying to delete\n", fn, status2);
    }

    return status;
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
        print_stderr("cp: missing file operand\n");
        NtTerminateThread(NtCurrentThread(), nt_status::INVALID_PARAMETER);
        unreachable();
    }

    if (argv.size() < 2) {
        print_stderr("cp: missing destination file operand after '{}'\n", argv[0]);
        NtTerminateThread(NtCurrentThread(), nt_status::INVALID_PARAMETER);
        unreachable();
    }

    nt_status ret;

    if (argv.size() > 2) {
        print_stderr("FIXME: copying multiple files\n"); // FIXME
        ret = nt_status::NOT_IMPLEMENTED;
    } else
        ret = cp(argv[0], argv[1]);

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    NtTerminateThread(NtCurrentThread(), ret);
}
