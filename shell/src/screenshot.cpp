#include <nt.h>
#include <ntdll.h>
#include <string_view>
#include <span>
#include <string.h>
#include <png.h>

import shelllib;
import print;

using namespace std;

static const uint16_t BMP_MAGIC = 0x4d42;

struct BITMAPFILEHEADER {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} __attribute((packed));

static_assert(sizeof(BITMAPFILEHEADER) == 14);

struct BITMAPINFOHEADER {
    uint32_t biSize;
    uint32_t biWidth;
    uint32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    uint32_t biXPelsPerMeter;
    uint32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
};

static_assert(sizeof(BITMAPINFOHEADER) == 40);

extern "C"
void* malloc(size_t size) {
    return RtlAllocateHeap(GetProcessHeap(), 0, size);
}

extern "C"
void free(void* ptr) {
    RtlFreeHeap(GetProcessHeap(), 0, ptr);
}

extern "C"
void abort() {
    NtTerminateProcess(NtCurrentProcess(), (NTSTATUS)0xffffffff);
    __builtin_unreachable();
}

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
                           span<const uint8_t> framebuffer) {
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

    png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    for (unsigned int i = 0; i < height; i++) {
        png_write_row(png, (uint8_t*)framebuffer.data()); // FIXME
    }

    png_write_end(png, NULL);

    png_destroy_write_struct(&png, &info);
}

static void take_screenshot() {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    HANDLE sect, file;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    void* addr;
    size_t size;
    // BITMAPFILEHEADER bmp1;
    // BITMAPINFOHEADER bmp2;

    static const u16string_view fb = u"\\dev\\framebuffer";
    static const u16string_view fn = u"screenshot-out.png";

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

    NtClose(sect);

    // save to file

    // FIXME - put timestamp in filename

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

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

    // FIXME - get from kernel
    uint32_t image_width = 1280;
    uint32_t image_height = 800;

    write_png_file(file, /*image_width*/1, /*image_height*/1, framebuffer);

#if 0
    bmp1.bfType = BMP_MAGIC;
    bmp1.bfSize = (uint32_t)(sizeof(bmp1) + sizeof(bmp2) + (image_width * image_height * sizeof(uint32_t)));
    bmp1.bfReserved1 = 0;
    bmp1.bfReserved2 = 0;
    bmp1.bfOffBits = sizeof(bmp1) + sizeof(bmp2);

    bmp2.biSize = sizeof(bmp2);
    bmp2.biWidth = image_width;
    bmp2.biHeight = -image_height; // negative because top-down
    bmp2.biPlanes = 1;
    bmp2.biBitCount = 32;
    bmp2.biCompression = 0;
    bmp2.biSizeImage = image_width * image_height * sizeof(uint32_t);
    bmp2.biXPelsPerMeter = 2835;
    bmp2.biYPelsPerMeter = 2835;
    bmp2.biClrUsed = 0;
    bmp2.biClrImportant = 0;

    status = NtWriteFile(file, 0, nullptr, nullptr, &iosb, &bmp1,
                         sizeof(bmp1), nullptr, nullptr);
    if (!success(status)) {
        print("NtWriteFile returned {}\n", status);
        NtClose(file);
        NtUnmapViewOfSection(NtCurrentProcess(), addr);
        return;
    }

    status = NtWriteFile(file, 0, nullptr, nullptr, &iosb, &bmp2,
                         sizeof(bmp2), nullptr, nullptr);
    if (!success(status)) {
        print("NtWriteFile returned {}\n", status);
        NtClose(file);
        NtUnmapViewOfSection(NtCurrentProcess(), addr);
        return;
    }

    // FIXME - do writes in one call once we can
    auto buf = framebuffer;
    while (!buf.empty()) {
        auto to_write = (uint32_t)min((size_t)4096, buf.size());

        status = NtWriteFile(file, 0, nullptr, nullptr, &iosb, buf.data(),
                             to_write, nullptr, nullptr);
        if (!success(status)) {
            print("NtWriteFile returned {}\n", status);
            NtClose(file);
            NtUnmapViewOfSection(NtCurrentProcess(), addr);
            return;
        }

        buf = span(buf.data() + iosb.Information, buf.size() - iosb.Information);
    }
#endif

    NtClose(file);

    NtUnmapViewOfSection(NtCurrentProcess(), addr);

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

    take_screenshot();

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    // FIXME - return value should be different if anything failed

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
