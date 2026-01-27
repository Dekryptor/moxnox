#include <span>
#include <string_view>
#include <string.h>

import nt;
import ntdll;
import cmdline;
import print;
import unique_handle;

using namespace std;

static nt_status kill(span<const u16string_view> argv) {
    if (argv.empty()) {
        print("usage: kill pid\n");
        return nt_status::INVALID_PARAMETER;
    }

    for (auto arg : argv) {
        auto orig_arg = arg;
        bool valid = true;
        uintptr_t pid = 0;
        OBJECT_ATTRIBUTES oa;
        HANDLE h;
        CLIENT_ID client_id;
        unique_handle proc;

        if (arg.empty())
            valid = false;
        else {
            while (!arg.empty()) {
                pid *= 10;

                if (arg.front() >= u'0' && arg.front() <= u'9')
                    pid += arg.front() - u'0';
                else {
                    valid = false;
                    break;
                }

                arg = u16string_view(arg.data() + 1, arg.size() - 1);
            }
        }

        if (!valid) {
            print_stderr("kill: {}: arguments must be process IDs\n", orig_arg);
            continue;
        }

        client_id.UniqueProcess = pid;
        client_id.UniqueThread = 0;

        memset(&oa, 0, sizeof(oa));
        oa.Length = sizeof(OBJECT_ATTRIBUTES);

        auto status = NtOpenProcess(&h, PROCESS_TERMINATE, &oa, &client_id);
        if (status == nt_status::INVALID_CID) {
            print_stderr("kill: ({}) - no such process\n", pid);
            continue;
        } else if (!success(status)) {
            print_stderr("NtOpenProcess returned {}\n", status);
            continue;
        }

        proc.reset(h);

        status = NtTerminateProcess(proc.get(), nt_status::CONTROL_C_EXIT);
        if (!success(status))
            print_stderr("NtTerminateProcess returned {}\n", status);
    }

    // FIXME - different status if we fail?

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

    auto status = kill(argv);

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    NtTerminateThread(NtCurrentThread(), status);
}
