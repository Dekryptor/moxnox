#include <span>
#include <string_view>
#include <chrono>
#include <sys/stat.h>

import nt;
import ntdll;
import print;
import ucrtbase;
import format;

#undef errno
import errno;

using namespace std;

static const size_t DEV_BSIZE = 4096;
static const size_t SECTOR_SIZE = 512;

struct time_wrapper {
    time_wrapper(time_t t) : t(t) { }

    time_t t;
};

struct permissions_wrapper {
    permissions_wrapper(uint32_t p) : p(p) { }

    uint32_t p;
};

template<>
consteval bool format_spec_valid<time_wrapper>(string_view fmt) {
    return fmt.empty();
}

static constexpr chrono::year_month_day system_time_to_ymd(uint64_t tm) noexcept {
    signed long long j, e, f, g, h;
    uint8_t day, month;
    uint16_t year;

    auto num = (int)(tm / (uint64_t)86400);

    j = num + 2440588;

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

static constexpr chrono::hh_mm_ss<chrono::seconds> system_time_to_hms(uint64_t tm) noexcept {
    auto secs = tm % (uint64_t)86400;

    return chrono::hh_mm_ss{chrono::seconds{secs}};
}

static span<char> do_format_arg(span<char> sp, string_view, const time_wrapper& t) {
    auto ymd = system_time_to_ymd(t.t);
    auto hms = system_time_to_hms(t.t);

    sp = do_format_arg(sp, ":04", (int)ymd.year());
    sp = do_format_arg(sp, "", "-");
    sp = do_format_arg(sp, ":02", (unsigned int)ymd.month());
    sp = do_format_arg(sp, "", "-");
    sp = do_format_arg(sp, ":02", (unsigned int)ymd.day());
    sp = do_format_arg(sp, "", " ");

    sp = do_format_arg(sp, ":02", hms.hours().count());
    sp = do_format_arg(sp, "", ":");
    sp = do_format_arg(sp, ":02", hms.minutes().count());
    sp = do_format_arg(sp, "", ":");
    sp = do_format_arg(sp, ":02", hms.seconds().count());

    // FIXME - put in local timezone, and print timezone offset

    return sp;
}

template<>
consteval bool format_spec_valid<permissions_wrapper>(string_view fmt) {
    return fmt.empty();
}

static span<char> do_format_arg(span<char> sp, string_view, const permissions_wrapper& p) {
    // FIXME - other file types
    sp = do_format_arg(sp, "", S_ISDIR(p.p) ? "d" : "-");

    sp = do_format_arg(sp, "", p.p & 0400 ?  "r" : "-");
    sp = do_format_arg(sp, "", p.p & 0200 ?  "w" : "-");
    sp = do_format_arg(sp, "", p.p & 0100 ?  "x" : "-"); // FIXME - s

    sp = do_format_arg(sp, "", p.p & 0040 ?  "r" : "-");
    sp = do_format_arg(sp, "", p.p & 0020 ?  "w" : "-");
    sp = do_format_arg(sp, "", p.p & 0010 ?  "x" : "-"); // FIXME - s

    sp = do_format_arg(sp, "", p.p & 0004 ?  "r" : "-");
    sp = do_format_arg(sp, "", p.p & 0002 ?  "w" : "-");
    sp = do_format_arg(sp, "", p.p & 0001 ?  "x" : "-"); // FIXME - t

    return sp;
}

static string_view filetype(uint32_t mode) {
    // FIXME - other file types

    if (S_ISDIR(mode))
        return "directory";
    else
        return "regular file";
}

static void stat_file(const char* fn) {
    struct stat st;

    static const uint32_t CHMOD_MODE_BITS = 07777;

    auto ret = stat(fn, &st);
    if (ret != 0) {
        print_stderr("stat returned {} (errno {})\n", ret,
                     errno_to_string_view(*_errno()));
        return;
    }

    auto blocks = ((st.st_size + DEV_BSIZE - 1) / DEV_BSIZE) * (DEV_BSIZE / SECTOR_SIZE);

    print("  File: {}\n", fn);
    print("  Size: {:<10}\tBlocks: {:<10} IO Block: {:<6} {}\n", st.st_size,
          blocks, DEV_BSIZE, filetype(st.st_mode));
    print("Device: {}\tInode: {:<10}  Links: {}\n", st.st_dev, st.st_ino,
          st.st_nlink); // FIXME - major and minor for device?
    print("Access: ({:04o}/{})  Uid: ({:5})   Gid: ({:5})\n", st.st_mode & CHMOD_MODE_BITS,
          permissions_wrapper(st.st_mode), st.st_uid, st.st_gid); // FIXME - username, group name
    print("Access: {}\n", time_wrapper(st.st_atime));
    print("Modify: {}\n", time_wrapper(st.st_mtime));
    print("Change: {}\n", time_wrapper(st.st_ctime));
}

int main(int argc, char** argv) {
    if (argc == 1) {
        print_stderr("stat: missing operand\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == 0)
            continue;

        stat_file(argv[i]);
    }

    // FIXME - return value should be different if anything failed

    return 0;
}
