#include <nt.h>
#include <ntdll.h>
#include <string_view>
#include <span>

import shelllib;
import unicode;

using namespace std;

void print(u16string_view sv) {
    auto len = utf16_to_utf8_len(sv);
    if (len == 0)
        return;

    auto s = (char*)RtlAllocateHeap(GetProcessHeap(), 0, len);
    if (!s)
        NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);

    utf16_to_utf8(span(s, len), sv);

    print(string_view(s, len));

    RtlFreeHeap(GetProcessHeap(), 0, s);
}

extern "C"
void startup() {
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
        if (i != 1)
            print(" ");

        print(argv[i]);
    }

    print("\n");

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
