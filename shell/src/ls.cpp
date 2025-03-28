#include <nt.h>
#include <ntdll.h>
#include <string_view>
#include <span>

using namespace std;

import shelllib;

static unsigned int count_cmdline_args(u16string_view cmdline) {
    unsigned int argc = 0;

    while (!cmdline.empty()) {
        while (cmdline.front() == ' ' || cmdline.front() == '\t') {
            cmdline = u16string_view(cmdline.data() + 1, cmdline.size() - 1);
        }

        if (cmdline.empty())
            break;

        size_t end = cmdline.size();

        if (cmdline.front() == u'"') {
            if (auto q = cmdline.find(u'"', 1); q != string_view::npos && q < end) {
                end = q - 1;
                cmdline = u16string_view(cmdline.data() + end + 2, cmdline.size() - end - 2);
            } else
                cmdline = u"";
        } else {
            if (auto sp = cmdline.find(u' '); sp != string_view::npos && sp < end)
                end = sp;

            if (auto tab = cmdline.find(u'\t'); tab != string_view::npos && tab < end)
                end = tab;

            cmdline = u16string_view(cmdline.data() + end, cmdline.size() - end);
        }

        argc++;
    }

    return argc;
}

static void parse_cmdline(u16string_view cmdline, span<u16string_view> argv) {
    // FIXME - escaping quotes with backslashes

    while (!cmdline.empty() && !argv.empty()) {
        auto& arg = argv.front();

        while (cmdline.front() == ' ' || cmdline.front() == '\t') {
            cmdline = u16string_view(cmdline.data() + 1, cmdline.size() - 1);
        }

        if (cmdline.empty())
            break;

        size_t end = cmdline.size();

        if (cmdline.front() == u'"') {
            if (auto q = cmdline.find(u'"', 1); q != string_view::npos && q < end) {
                end = q - 1;
                arg = u16string_view(cmdline.data() + 1, end);
                cmdline = u16string_view(cmdline.data() + end + 2, cmdline.size() - end - 2);
            } else {
                arg = u16string_view(cmdline.data() + 1, end - 1);
                cmdline = u"";
            }
        } else {
            if (auto sp = cmdline.find(u' '); sp != string_view::npos && sp < end)
                end = sp;

            if (auto tab = cmdline.find(u'\t'); tab != string_view::npos && tab < end)
                end = tab;

            arg = u16string_view(cmdline.data(), end);
            cmdline = u16string_view(cmdline.data() + end, cmdline.size() - end);
        }

        argv = argv.subspan(1);
    }
}

static void ls() {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;
    char buf[4096];

    bool first = true;

    while (true) {
        auto status = NtQueryDirectoryFile(peb->ProcessParameters->CurrentDirectory.Handle, 0, nullptr, nullptr,
                                           &iosb, buf, sizeof(buf), FILE_INFORMATION_CLASS::DirectoryInformation,
                                           false, nullptr, first);

        if (status == nt_status::NO_MORE_FILES)
            break;
        else if (!success(status)) {
            print("NtQueryDirectoryFile returned "); // FIXME - should be to stderr
            print_hex((uint32_t)status, 8);
            print("\n");
            NtTerminateThread(NtCurrentThread(), status);
            return;
        }

        auto& fdi = *(FILE_DIRECTORY_INFORMATION*)buf;
        auto u16sv = u16string_view(fdi.FileName, fdi.FileNameLength / sizeof(char16_t));
        auto len = utf16_to_utf8_len(u16sv);
        char s[255];

        utf16_to_utf8(s, u16sv);

        // FIXME - print times and file sizes (and attributes?)

        if (fdi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            print("\e[01;34m");

        print(string_view(s, len));

        if (fdi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            print("\e[0m");

        print("\n");

        first = false;
    }
}

extern "C"
void startup() {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;

    auto cmdline = u16string_view(peb->ProcessParameters->CommandLine.Buffer,
                                  peb->ProcessParameters->CommandLine.Length / sizeof(char16_t));

    // FIXME - directory as parameter
    // FIXME - wildcard as parameter

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

    ls();

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
