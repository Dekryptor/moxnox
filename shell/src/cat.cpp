#include <nt.h>
#include <ntdll.h>
#include <string_view>
#include <span>
#include <string.h>

import shelllib;
import print;

using namespace std;

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

extern "C"
void* malloc(size_t size) {
    return RtlAllocateHeap(GetProcessHeap(), 0, size);
}

extern "C"
void free(void* ptr) {
    RtlFreeHeap(GetProcessHeap(), 0, ptr);
}

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
        print("cat: {}: No such file or directory\n", fn); // FIXME - stderr
        return;
    } else if (!success(status)) {
        print("cat: {}: NtCreateFile returned {}\n", fn, status); // FIXME - stderr
        return;
    }

    while (true) {
        char buf[4096];

        auto status = NtReadFile(file, 0, nullptr, nullptr, &iosb,
                                 buf, sizeof(buf), nullptr, nullptr);

        if (status == nt_status::END_OF_FILE)
            break;

        if (!success(status)) {
            print("cat: {}: NtReadFile returned {}\n", fn, status); // FIXME - stderr
            break;
        }

        if (iosb.Information == 0)
            continue;

        status = NtWriteFile(peb->ProcessParameters->StandardOutput, 0,
                             nullptr, nullptr, &iosb, buf,
                             (uint32_t)iosb.Information, nullptr, nullptr);
        if (!success(status)) {
            print("cat: {}: NtWriteFile returned {}\n", fn, status); // FIXME - stderr
            break;
        }
    }

    NtClose(file);
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
        cat_file(argv[i]);
    }

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    // FIXME - return value should be different if anything failed

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
