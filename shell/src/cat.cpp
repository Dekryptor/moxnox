#include <string_view>
#include <span>
#include <string.h>
#include <stdint.h>

import nt;
import ntdll;
import print;
import cmdline;

using namespace std;

static void cat_file(u16string_view fn) {
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

    status = NtCreateFile(&file, FILE_READ_DATA | SYNCHRONIZE, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_SYNCHRONOUS_IO_ALERT, nullptr, 0);

    if (status == nt_status::OBJECT_NAME_NOT_FOUND) {
        print_stderr("cat: {}: No such file or directory\n", fn);
        return;
    } else if (!success(status)) {
        print_stderr("cat: {}: NtCreateFile returned {}\n", fn, status);
        return;
    }

    while (true) {
        char buf[4096];
        uint64_t read_bytes;

        auto status = NtReadFile(file, 0, nullptr, nullptr, &iosb,
                                 buf, sizeof(buf), nullptr, nullptr);

        if (status == nt_status::END_OF_FILE)
            break;

        if (!success(status)) {
            print_stderr("cat: {}: NtReadFile returned {}\n", fn, status);
            break;
        }

        read_bytes = iosb.Information;

        if (read_bytes == 0)
            continue;

        span sp(buf, read_bytes);

        do {
            status = NtWriteFile(peb->ProcessParameters->StandardOutput, 0,
                                 nullptr, nullptr, &iosb, sp.data(),
                                 (uint32_t)sp.size(), nullptr, nullptr);
            if (!success(status)) {
                print_stderr("cat: {}: NtWriteFile returned {}\n", fn, status);
                break;
            }

            if (iosb.Information == sp.size())
                break;

            sp = span(sp.data() + iosb.Information, sp.size() - iosb.Information);
        } while (true);

        if (!success(status))
            break;
    }

    NtClose(file);
}

static void cat_file_mmap(u16string_view fn) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE file, section;
    IO_STATUS_BLOCK iosb;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    void* addr;
    size_t size;
    FILE_STANDARD_INFORMATION fsi;

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&file, FILE_READ_DATA | SYNCHRONIZE, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_SYNCHRONOUS_IO_ALERT, nullptr, 0);

    if (status == nt_status::OBJECT_NAME_NOT_FOUND) {
        print_stderr("cat: {}: No such file or directory\n", fn);
        return;
    } else if (!success(status)) {
        print_stderr("cat: {}: NtCreateFile returned {}\n", fn, status);
        return;
    }

    status = NtQueryInformationFile(file, &iosb, &fsi, sizeof(fsi),
                                    FILE_INFORMATION_CLASS::StandardInformation);
    if (!success(status)) {
        print_stderr("cat: {}: NtQueryInformationFile returned {}\n", fn, status);
        NtClose(file);
        return;
    }

    status = NtCreateSection(&section, SECTION_MAP_READ, nullptr, nullptr,
                             PAGE_READONLY, SEC_COMMIT, file);
    if (!success(status)) {
        print_stderr("cat: {}: NtCreateSection returned {}\n", fn, status);
        NtClose(file);
        return;
    }

    NtClose(file);

    addr = nullptr;
    size = 0;

    status = NtMapViewOfSection(section, NtCurrentProcess(), &addr, 0, 0, nullptr,
                                &size, SECTION_INHERIT::ViewUnmap, 0, PAGE_READONLY);
    if (!success(status)) {
        print_stderr("cat: {}: NtMapViewOfSection returned {}\n", fn, status);
        return;
    }

    NtClose(section);

    span sp((uint8_t*)addr, (size_t)fsi.EndOfFile.QuadPart);

    // FIXME - write whole thing at once?
    while (!sp.empty()) {
        status = NtWriteFile(peb->ProcessParameters->StandardOutput, 0,
                             nullptr, nullptr, &iosb, sp.data(),
                             min((uint32_t)sp.size(), (uint32_t)4096), nullptr,
                             nullptr);
        if (!success(status)) {
            print_stderr("cat: {}: NtWriteFile returned {}\n", fn, status);
            break;
        }

        sp = span(sp.data() + iosb.Information, sp.size() - iosb.Information);
    }

    status = NtUnmapViewOfSection(NtCurrentProcess(), addr);
    if (!success(status)) {
        print_stderr("cat: {}: NtUnmapViewOfSection returned {}\n", fn, status);
        return;
    }
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
    bool mmap_mode = false;

    for (size_t i = 1; i < argv.size(); i++) {
        if (argv[i] == u"-m") {
            mmap_mode = true;
            continue;
        }

        if (mmap_mode) // TESTING
            cat_file_mmap(argv[i]);
        else
            cat_file(argv[i]);
    }

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    // FIXME - return value should be different if anything failed

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
