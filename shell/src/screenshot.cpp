#include <string_view>
#include <span>
#include <chrono>
#include <string.h>
#include <png.h>

import nt;
import ntdll;
import print;
import format;
import unicode;
import cmdline;

using namespace std;

static void write_png(png_structp png, png_bytep buf, size_t size) {
    IO_STATUS_BLOCK iosb;

    auto file = (HANDLE)png_get_io_ptr(png);

    auto status = NtWriteFile(file, 0, nullptr, nullptr, &iosb, buf,
                              (uint32_t)size, nullptr, nullptr);
    if (!success(status))
        print("NtWriteFile returned {}\n", status);
}

static void flush_png(png_structp) {
}

static void png_error(png_structp, const char* msg) {
    print("libpng error: {}\n", msg);
}

static void write_png_file(HANDLE file, unsigned int width, unsigned int height,
                           span<const uint8_t> framebuffer,
                           unsigned int pixels_per_scan_line) {
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                              nullptr, nullptr);
    if (!png) {
        print("png_create_write_struct failed\n");
        return;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        print("png_create_info_struct failed\n");
        return;
    }

    png_set_write_fn(png, (png_voidp)file, write_png, flush_png);
    png_set_error_fn(png, nullptr, png_error, nullptr);
    png_set_bgr(png);

    png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    png_set_filler(png, 0, PNG_FILLER_AFTER);

    auto ptr = framebuffer.data();
    for (unsigned int i = 0; i < height; i++) {
        png_write_row(png, ptr);

        ptr += pixels_per_scan_line * sizeof(uint32_t);
    }

    png_write_end(png, NULL);

    png_destroy_write_struct(&png, &info);
}

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

static bool get_screenshot_filename(span<char16_t> buf, UNICODE_STRING& us) {
    LARGE_INTEGER time;
    auto status = NtQuerySystemTime(&time);
    char s[50];

    if (!success(status)) {
        print("NtQuerySystemTime returned {}\n", status);
        return false;
    }

    auto ymd = system_time_to_ymd(time.QuadPart);
    auto hms = system_time_to_hms(time.QuadPart);

    auto sp = span(s, sizeof(s));

    sp = format(sp, "screenshot-{:04}{:02}{:02}-{:02}{:02}{:02}.png",
                (int)ymd.year(), (unsigned int)ymd.month(), (unsigned int)ymd.day(),
                hms.hours().count(), hms.minutes().count(), hms.seconds().count());

    auto sv = string_view(s, (size_t)(sp.data() - s));

    utf8_to_utf16(buf, sv);

    us.Length = us.MaximumLength = (uint16_t)(sv.size() * sizeof(char16_t));
    us.Buffer = buf.data();

    return true;
}

static void take_screenshot() {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    HANDLE sect, file;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    void* addr;
    size_t size, return_length;
    MN_SECTION_FRAMEBUFFER_INFORMATION fb_info;
    char16_t fn[50];

    static const u16string_view fb = u"\\dev\\framebuffer";

    us.Length = us.MaximumLength = (uint16_t)(fb.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fb.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtOpenSection(&sect, SECTION_MAP_READ, &oa);
    if (!success(status)) {
        print("NtOpenSection returned {}\n", status);
        return;
    }

    addr = nullptr;
    size = 0;

    status = NtMapViewOfSection(sect, NtCurrentProcess(), &addr, 0, 0, nullptr,
                                &size, SECTION_INHERIT::ViewUnmap, 0, PAGE_READONLY);
    if (!success(status)) {
        print("NtMapViewOfSection returned {}\n", status);
        return;
    }

    status = NtQuerySection(sect, SECTION_INFORMATION_CLASS::MoxnoxFramebufferInformation,
                            &fb_info, sizeof(fb_info), &return_length);
    if (!success(status)) {
        print("NtQuerySection returned {}\n", status);
        return;
    }

    NtClose(sect);

    // save to file

    if (!get_screenshot_filename(fn, us))
        return;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&file, FILE_WRITE_DATA, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, 0, FILE_CREATE,
                          0, nullptr, 0);

    if (!success(status)) {
        print("NtCreateFile returned {}\n", status); // FIXME - stderr
        NtUnmapViewOfSection(NtCurrentProcess(), addr);
        return;
    }

    span framebuffer((uint8_t*)addr, size / sizeof(uint8_t));

    uint32_t image_width = fb_info.width;
    uint32_t image_height = fb_info.height;
    uint32_t pixels_per_scan_line = fb_info.pixels_per_scan_line;

    write_png_file(file, image_width, image_height, framebuffer,
                   pixels_per_scan_line);

    NtClose(file);

    NtUnmapViewOfSection(NtCurrentProcess(), addr);

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

    take_screenshot();

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    // FIXME - return value should be different if anything failed

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
