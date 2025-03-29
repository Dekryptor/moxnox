#include <nt.h>
#include <ntdll.h>
#include <string_view>
#include <span>

using namespace std;

import shelllib;
import format;
import print;
import list;

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

struct ls_entry {
    list_entry le;
    FILE_DIRECTORY_INFORMATION fdi;
};

static void ls(bool long_mode) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;
    char buf[4096];

    bool first = true;

    list_entry list = { &list, &list };

    while (true) {
        auto status = NtQueryDirectoryFile(peb->ProcessParameters->CurrentDirectory.Handle, 0, nullptr, nullptr,
                                           &iosb, buf, sizeof(buf), FILE_INFORMATION_CLASS::DirectoryInformation,
                                           false, nullptr, first);

        if (status == nt_status::NO_MORE_FILES)
            break;
        else if (!success(status)) {
            print("NtQueryDirectoryFile returned {}\n", status); // FIXME - should be to stderr
            NtTerminateThread(NtCurrentThread(), status);
            return;
        }

        auto& fdi = *(FILE_DIRECTORY_INFORMATION*)buf;

        size_t ent_len = offsetof(ls_entry, fdi.FileName) + fdi.FileNameLength;

        auto ent = (ls_entry*)RtlAllocateHeap(GetProcessHeap(), 0, ent_len);
        if (!ent)
            NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);

        memcpy(&ent->fdi, &fdi, offsetof(FILE_DIRECTORY_INFORMATION, FileName) + fdi.FileNameLength);

        insert_tail_list(list, ent->le);

        first = false;
    }

    // FIXME - sort

    uint64_t max_size = 0;

    for (auto le = list.next; le != &list; le = le->next) {
        auto& ent = containing_record<ls_entry>(le);

        max_size = max(max_size, (uint64_t)ent.fdi.EndOfFile.QuadPart);
    }

    unsigned int size_padding = 0;
    while (max_size > 0) {
        size_padding++;
        max_size /= 10;
    }

    while (!is_list_empty(list)) {
        auto& ent = containing_record<ls_entry>(list.next);
        auto& fdi = ent.fdi;

        remove_entry_list(ent.le);

        auto u16sv = u16string_view(fdi.FileName, fdi.FileNameLength / sizeof(char16_t));

        if (long_mode) {
            // FIXME - permissions
            // FIXME - hardlink count(?)
            // FIXME - username
            // FIXME - group

            print("{:{}} ", fdi.EndOfFile.QuadPart, size_padding);

            // FIXME - date
        }

        if (fdi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            print("\e[01;34m");

        print("{}", u16sv);

        if (fdi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            print("\e[0m");

        print("\n");

        RtlFreeHeap(GetProcessHeap(), 0, &ent);
    }
}

extern "C"
void startup() {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    bool long_mode = false;

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

    // FIXME - options, then --, then files or directories

    for (auto arg : argv) {
        if (arg.empty())
            continue;

        // FIXME - die if unrecognized option

        if (arg == u"-l")
            long_mode = true;
    }

    ls(long_mode);

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
