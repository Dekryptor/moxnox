#include <string_view>
#include <span>
#include <chrono>
#include <algorithm>
#include <utility>
#include <string.h>

using namespace std;

import nt;
import ntdll;
import shelllib;
import format;
import print;
import list;
import cmdline;
import getopt;

static bool long_mode = false;
static bool directory_flag = false;
static bool show_all = false;

static constexpr chrono::year_month_day system_time_to_ymd(int64_t system_time) noexcept {
    signed long long j, e, f, g, h;
    uint8_t day, month;
    uint16_t year;

    auto num = (int)(system_time / (int64_t)864000000000);

    j = num + 2305814;

    f = (4 * j) + 274277;
    f /= 146097;
    f *= 3;
    f /= 4;
    f += j;
    f += 1363;

    e = (4 * f) + 3;
    g = (e % 1461) / 4;
    h = (5 * g) + 2;

    day = (uint8_t)(((h % 153) / 5) + 1);
    month = (uint8_t)(((h / 153) + 2) % 12 + 1);
    year = static_cast<uint16_t>((e / 1461) - 4716 + ((14 - month) / 12));

    return {chrono::year{year}, chrono::month{month}, chrono::day{day}};
}

static constexpr chrono::hh_mm_ss<chrono::seconds> system_time_to_hms(int64_t system_time) noexcept {
    auto secs = (unsigned int)((system_time % (int64_t)864000000000) / 10000000);

    return chrono::hh_mm_ss{chrono::seconds{secs}};
}

static void print_date(int64_t tm, int64_t now) {
    auto ymd = system_time_to_ymd(tm);
    auto hms = system_time_to_hms(tm);

    static const char* months[] = {
        "Jan",
        "Feb",
        "Mar",
        "Apr",
        "May",
        "Jun",
        "Jul",
        "Aug",
        "Sep",
        "Oct",
        "Nov",
        "Dec"
    };

    static const int64_t SIX_MONTHS = 157784760000000;

    if (tm <= now && tm >= now - SIX_MONTHS) {
        print("{} {:2} {:02}:{:02}", months[(unsigned int)ymd.month() - 1], (unsigned int)ymd.day(),
              hms.hours().count(), hms.minutes().count());
    } else {
        print("{} {:2}  {}", months[(unsigned int)ymd.month() - 1], (unsigned int)ymd.day(),
              (int)ymd.year());
    }
}

struct ls_entry {
    list_entry le;
    FILE_DIRECTORY_INFORMATION fdi;
};

static void print_file_list(list_entry& list, unsigned int num_entries) {
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

    auto arr = (ls_entry**)RtlAllocateHeap(GetProcessHeap(), 0, sizeof(ls_entry*) * num_entries);
    if (!arr) {
        NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
        unreachable();
    }

    {
        auto ptr = arr;

        for (auto le = list.next; le != &list; le = le->next) {
            auto& ent = containing_record<ls_entry>(le);

            *ptr = &ent;
            ptr++;
        }
    }

    ranges::sort(span(arr, num_entries), [](ls_entry* a, ls_entry* b) -> bool {
        auto name1 = u16string_view(a->fdi.FileName, a->fdi.FileNameLength / sizeof(char16_t));
        auto name2 = u16string_view(b->fdi.FileName, b->fdi.FileNameLength / sizeof(char16_t));

        // FIXME - should be case-insensitive comparison

        return name1 < name2;
    });

    LARGE_INTEGER now;

    if (auto status = NtQuerySystemTime(&now); !success(status)) {
        NtTerminateThread(NtCurrentThread(), status);
        unreachable();
    }

    for (unsigned int i = 0; i < num_entries; i++) {
        auto& ent = *arr[i];
        auto& fdi = ent.fdi;

        auto u16sv = u16string_view(fdi.FileName, fdi.FileNameLength / sizeof(char16_t));

        // FIXME - also skip files when HIDDEN bit set?
        if (!show_all && !u16sv.empty() && u16sv[0] == '.')
            continue;

        if (long_mode) {
            // FIXME - permissions
            // FIXME - hardlink count(?)
            // FIXME - username
            // FIXME - group

            print("{:{}} ", fdi.EndOfFile.QuadPart, size_padding);

            print_date(fdi.LastWriteTime.QuadPart, now.QuadPart);
            print(" ");
        }

        if (fdi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            print("\e[01;34m");

        print("{}", u16sv);

        if (fdi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            print("\e[0m");

        print("\n");
    }

    RtlFreeHeap(GetProcessHeap(), 0, arr);
}

static void ls_dir(HANDLE h) {
    IO_STATUS_BLOCK iosb;
    char buf[4096];
    unsigned int num_entries = 0;
    list_entry list = { &list, &list };

    while (true) {
        auto status = NtQueryDirectoryFile(h, 0, nullptr, nullptr,
                                           &iosb, buf, sizeof(buf), FILE_INFORMATION_CLASS::DirectoryInformation,
                                           false, nullptr, num_entries == 0 ? true : false);

        if (status == nt_status::NO_MORE_FILES)
            break;
        else if (!success(status)) {
            print("NtQueryDirectoryFile returned {}\n", status); // FIXME - should be to stderr
            NtTerminateThread(NtCurrentThread(), status);
            unreachable();
        }

        auto fdi = (FILE_DIRECTORY_INFORMATION*)buf;

        while (true) {
            size_t ent_len = offsetof(ls_entry, fdi.FileName) + fdi->FileNameLength;

            auto ent = (ls_entry*)RtlAllocateHeap(GetProcessHeap(), 0, ent_len);
            if (!ent) {
                NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
                unreachable();
            }

            memcpy(&ent->fdi, fdi, offsetof(FILE_DIRECTORY_INFORMATION, FileName) + fdi->FileNameLength);

            insert_tail_list(list, ent->le);

            num_entries++;

            if (fdi->NextEntryOffset == 0)
                break;

            fdi = (FILE_DIRECTORY_INFORMATION*)((uint8_t*)fdi + fdi->NextEntryOffset);
        }
    }

    if (is_list_empty(list))
        return;

    print_file_list(list, num_entries);

    while (!is_list_empty(list)) {
        auto& e = containing_record<ls_entry>(list.next);

        remove_entry_list(e.le);
        free(&e);
    }
}

static void ls(span<u16string_view> argv) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    struct dir_entry {
        list_entry le;
        u16string_view fn;
        HANDLE h;
    };

    list_entry dir_list = { &dir_list, &dir_list };
    list_entry file_list = { &file_list, &file_list };
    unsigned int num_file_entries = 0;

    auto do_arg = [&dir_list, &file_list, &num_file_entries, &peb](u16string_view arg) {
        OBJECT_ATTRIBUTES oa;
        UNICODE_STRING us;
        NTSTATUS status;
        HANDLE h;
        IO_STATUS_BLOCK iosb;
        FILE_BASIC_INFORMATION fbi;

        // FIXME - unlike Linux, NT kernel doesn't seem to resolve .. etc.

        us.Length = us.MaximumLength = (uint16_t)(arg.size() * sizeof(char16_t));
        us.Buffer = (char16_t*)arg.data();

        memset(&oa, 0, sizeof(oa));
        oa.Length = sizeof(OBJECT_ATTRIBUTES);
        oa.ObjectName = &us;
        oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

        status = NtCreateFile(&h, FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                              &oa, &iosb, nullptr, 0,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              FILE_OPEN, FILE_SYNCHRONOUS_IO_ALERT, nullptr, 0);

        if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
            status == nt_status::OBJECT_PATH_NOT_FOUND) {
            print("ls: {}: No such file or directory\n", arg); // FIXME - stderr
            return;
        } else if (!success(status)) {
            print("ls: {}: NtCreateFile returned {}\n", arg, status); // FIXME - stderr
            return;
        }

        status = NtQueryInformationFile(h, &iosb, &fbi, sizeof(fbi),
                                        FILE_INFORMATION_CLASS::BasicInformation);
        if (!success(status)) {
            print("ls: {}: NtQueryInformationFile returned {}\n", arg, status); // FIXME - stderr
            NtClose(h);
            return;
        }

        if (fbi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY && !directory_flag) {
            auto e = (dir_entry*)malloc(sizeof(dir_entry));
            if (!e) {
                NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
                unreachable();
            }

            e->fn = arg;
            e->h = h;

            insert_tail_list(dir_list, e->le);
        } else {
            size_t len = offsetof(ls_entry, fdi.FileName) + (arg.size() * sizeof(char16_t));
            auto e = (ls_entry*)malloc(len);
            if (!e) {
                NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
                unreachable();
            }

            auto& fdi = e->fdi;

            if (long_mode) {
                FILE_STANDARD_INFORMATION fsi;

                status = NtQueryInformationFile(h, &iosb, &fsi, sizeof(fsi),
                                                FILE_INFORMATION_CLASS::StandardInformation);
                if (!success(status)) {
                    print("ls: {}: NtQueryInformationFile returned {}\n", arg, status); // FIXME - stderr
                    NtClose(h);
                    free(e);
                    return;
                }

                fdi.EndOfFile = fsi.EndOfFile;
                fdi.AllocationSize = fsi.AllocationSize;
            }

            fdi.CreationTime = fbi.CreationTime;
            fdi.LastAccessTime = fbi.LastAccessTime;
            fdi.LastWriteTime = fbi.LastWriteTime;
            fdi.ChangeTime = fbi.ChangeTime;
            fdi.FileAttributes = fbi.FileAttributes;
            fdi.FileNameLength = (uint32_t)(arg.size() * sizeof(char16_t));
            memcpy(fdi.FileName, arg.data(), arg.size() * sizeof(char16_t));

            num_file_entries++;
            insert_tail_list(file_list, e->le);

            NtClose(h);
        }
    };

    if (argv.empty())
        do_arg(u".");
    else {
        for (auto arg : argv) {
            do_arg(arg);
        }
    }

    // only one entry, which is directory
    if (is_list_empty(file_list) && !is_list_empty(dir_list) &&
        dir_list.next->next == &dir_list) {
        auto& e = containing_record<dir_entry>(dir_list.next);

        ls_dir(e.h);
        NtClose(e.h);

        free(&e);
        return;
    }

    bool first = true;

    if (!is_list_empty(file_list)) {
        print_file_list(file_list, num_file_entries);
        first = false;
    }

    while (!is_list_empty(file_list)) {
        auto& e = containing_record<ls_entry>(file_list.next);

        remove_entry_list(e.le);
        free(&e);
    }

    while (!is_list_empty(dir_list)) {
        auto& e = containing_record<dir_entry>(dir_list.next);

        if (!first)
            print("\n");

        print("{}:\n", e.fn);
        ls_dir(e.h);

        NtClose(e.h);
        remove_entry_list(e.le);
        free(&e);

        first = false;
    }
}

#ifdef __x86_64__
#define FORCE_ALIGN_ARG_POINTER __attribute__((force_align_arg_pointer))
#else
#define FORCE_ALIGN_ARG_POINTER
#endif

extern "C" FORCE_ALIGN_ARG_POINTER void startup() {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;

    static const option options[] = {
        { u"", u'l', []{ long_mode = true; } },
        { u"directory", u'd', []{ directory_flag = true; } },
        { u"all", u'a', []{ show_all = true; } },
    };

    auto cmdline = u16string_view(peb->ProcessParameters->CommandLine.Buffer,
                                  peb->ProcessParameters->CommandLine.Length / sizeof(char16_t));

    auto argc = count_cmdline_args(cmdline);
    span<u16string_view> argv;
    u16string_view* buf = nullptr;

    if (argc != 0) {
        buf = (decltype(buf))RtlAllocateHeap(GetProcessHeap(), 0, argc * sizeof(*buf));
        if (!buf) {
            NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
            unreachable();
        }

        argv = span(buf, argc);
    }

    parse_cmdline(cmdline, argv);

    if (!argv.empty()) // skip program name
        argv = span(argv.data() + 1, argv.size() - 1);

    if (!getopt(argv, options)) {
        if (buf)
            RtlFreeHeap(GetProcessHeap(), 0, buf);

        NtTerminateThread(NtCurrentThread(), nt_status::INVALID_PARAMETER);
        unreachable();
    }

    ls(argv);

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
    unreachable();
}
