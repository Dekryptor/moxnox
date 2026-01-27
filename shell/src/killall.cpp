#include <span>
#include <string_view>
#include <string.h>
#include <stdint.h>

import nt;
import ntdll;
import cmdline;
import print;
import unique_handle;

using namespace std;

static nt_status killall(span<const u16string_view> argv) {
    unique_handle proc;
    UNICODE_STRING us;
    uint32_t retlen;

    if (argv.empty()) {
        print("usage: killall NAME\n");
        return nt_status::INVALID_PARAMETER;
    }

    auto done = (bool*)RtlAllocateHeap(GetProcessHeap(), 0, sizeof(bool) * argv.size());
    if (!done) {
        print_stderr("RtlAllocateHeap failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    memset(done, 0, sizeof(bool) * argv.size());

    while (true) {
        HANDLE h;

        auto status = NtGetNextProcess(proc.get(), PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE,
                                       0, 0, &h);
        if (status == nt_status::NO_MORE_ENTRIES)
            break;
        else if (!success(status)) {
            print_stderr("NtGetNextProcess returned {}\n", status);
            RtlFreeHeap(GetProcessHeap(), 0, done);
            return status;
        }

        proc.reset(h);

        status = NtQueryInformationProcess(proc.get(), PROCESSINFOCLASS::ImageFileName,
                                           &us, sizeof(us), &retlen);
        if (!success(status) && status != nt_status::INFO_LENGTH_MISMATCH) {
            print_stderr("NtQueryInformationProcess returned {}\n", status);
            RtlFreeHeap(GetProcessHeap(), 0, done);
            return status;
        }

        auto buffer = RtlAllocateHeap(GetProcessHeap(), 0, retlen);
        if (!buffer) {
            print_stderr("RtlAllocateHeap failed\n");
            RtlFreeHeap(GetProcessHeap(), 0, done);
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        status = NtQueryInformationProcess(proc.get(), PROCESSINFOCLASS::ImageFileName,
                                           buffer, retlen, &retlen);
        if (!success(status)) {
            RtlFreeHeap(GetProcessHeap(), 0, buffer);
            print_stderr("NtQueryInformationProcess returned {}\n", status);
            return status;
        }

        auto& us = *(UNICODE_STRING*)buffer;

        u16string_view name(us.Buffer, us.Length / sizeof(char16_t));

        if (auto bs = name.find_last_of(u'\\'); bs != u16string_view::npos)
            name = u16string_view(name.data() + bs + 1, name.size() - bs - 1);

        if (auto dot = name.find_last_of(u'.'); dot != u16string_view::npos)
            name = u16string_view(name.data(), dot);

        for (size_t i = 0; i < argv.size(); i++) {
            if (argv[i] == name) {
                auto status = NtTerminateProcess(proc.get(),
                                                 nt_status::CONTROL_C_EXIT);
                if (!success(status))
                    print_stderr("{}: NtTerminateProcess returned {}\n", name, status);

                done[i] = true;
                break;
            }
        }

        RtlFreeHeap(GetProcessHeap(), 0, buffer);
    }

    for (size_t i = 0; i < argv.size(); i++) {
        if (!done[i])
            print_stderr("{}: no process found\n", argv[i]);
    }

    RtlFreeHeap(GetProcessHeap(), 0, done);

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

    if (!argv.empty())
        argv = span(argv.data() + 1, argv.size() - 1);

    auto status = killall(argv);

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    NtTerminateThread(NtCurrentThread(), status);
}
