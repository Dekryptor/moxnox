#include <string_view>
#include <span>
#include <chrono>
#include <algorithm>
#include <utility>

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

static void ls() {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;
    char buf[4096];

    unsigned int num_entries = 0;

    list_entry list = { &list, &list };

    while (true) {
        auto status = NtQueryDirectoryFile(peb->ProcessParameters->CurrentDirectory.Handle, 0, nullptr, nullptr,
                                           &iosb, buf, sizeof(buf), FILE_INFORMATION_CLASS::DirectoryInformation,
                                           false, nullptr, num_entries == 0 ? true : false);

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

        num_entries++;
    }

    if (is_list_empty(list))
        return;

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
    if (!arr)
        NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);

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

    if (auto status = NtQuerySystemTime(&now); !success(status))
        NtTerminateThread(NtCurrentThread(), status);

    for (unsigned int i = 0; i < num_entries; i++) {
        auto& ent = *arr[i];
        auto& fdi = ent.fdi;

        auto u16sv = u16string_view(fdi.FileName, fdi.FileNameLength / sizeof(char16_t));

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

        RtlFreeHeap(GetProcessHeap(), 0, &ent);
    }

    RtlFreeHeap(GetProcessHeap(), 0, arr);
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
        { u"", u'l', []() { long_mode = true; } }
    };

    auto cmdline = u16string_view(peb->ProcessParameters->CommandLine.Buffer,
                                  peb->ProcessParameters->CommandLine.Length / sizeof(char16_t));

    // FIXME - directory as parameter
    // FIXME - wildcard as parameter

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

    ls();

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
    unreachable();
}
