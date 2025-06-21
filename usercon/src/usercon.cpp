#include <ntdll.h>
#include <string_view>
#include <tuple>
#include <span>
#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftbitmap.h>

import format;
import rbtree;

using namespace std;

struct glyph_info {
    // FIXME - destructor

    rbtree_node<FT_UInt> tree;
    FT_Int bitmap_left;
    FT_Int bitmap_top;
    FT_Bitmap bitmap;
};

struct font_info {
    FT_Face face;
    rbtree_node<FT_UInt>* glyphs = nullptr; // FIXME - destructor for this
};

static FT_Library ft;
static font_info font;
static MN_SECTION_FRAMEBUFFER_INFORMATION fb_info;
static span<uint32_t> framebuffer;

extern "C"
void* malloc(size_t size) {
    return RtlAllocateHeap(GetProcessHeap(), 0, size);
}

extern "C"
void free(void* ptr) {
    RtlFreeHeap(GetProcessHeap(), 0, ptr);
}

extern "C++"
void* operator new(size_t count) {
    return malloc(count);
}

extern "C++"
void operator delete(void* ptr) noexcept {
    free(ptr);
}

extern "C++"
void operator delete(void* ptr, size_t) noexcept {
    free(ptr);
}

static void* ft_alloc(FT_Memory, long size) {
    return malloc((size_t)size);
}

static void* ft_realloc(FT_Memory, long cur_size, long new_size, void* block) {
    void* ret;

    ret = malloc((size_t)new_size);
    if (!ret)
        return nullptr;

    memcpy(ret, block, cur_size < new_size ? (size_t)new_size : (size_t)cur_size);

    free(block);

    return ret;
}

static void ft_free(FT_Memory, void* block) {
    free(block);
}

extern "C"
FT_Memory FT_New_Memory() {
    auto ptr = (FT_MemoryRec_*)malloc(sizeof(FT_MemoryRec_));
    if (!ptr)
        return nullptr;

    auto& memory = *ptr;

    memory.user = nullptr;
    memory.alloc = ft_alloc;
    memory.realloc = ft_realloc;
    memory.free = ft_free;

    return &memory;
}

extern "C"
void FT_Done_Memory(FT_Memory memory) {
    free(memory);
}

template<typename... Args>
void print(type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args) {
    char buf[4096];
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    auto sp = span(buf, sizeof(buf));

    auto t = make_tuple(args...);

    sp = format_to(sp, fmt.sv, t);

    auto sv = string_view(buf, (size_t)(sp.data() - buf));

    NtWriteFile(peb->ProcessParameters->StandardOutput, 0, nullptr, nullptr,
                &iosb, sv.data(), (uint32_t)sv.size(), nullptr, nullptr);
}

static bool init_font(span<const uint8_t> buf) {
    FT_Error error;
    unsigned int font_size = 10; // FIXME
    unsigned int dpi = 72; // FIXME

    error = FT_Init_FreeType(&ft);
    if (error) {
        print("FT_Init_FreeType failed ({:x})\n", error);
        return false;
    }

    error = FT_New_Memory_Face(ft, buf.data(), (FT_Long)buf.size(),
                               0, &font.face);
    if (error) {
        print("FT_New_Memory_Face failed ({:x})\n", error);
        return false;
    }

    error = FT_Set_Char_Size(font.face, font_size * 64, 0, dpi, 0);
    if (error) {
        print("FT_Set_Char_Size failed ({:x})\n", error);
        return false;
    }

    return true;
}

static span<uint32_t> init_framebuffer() {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE sect;
    void* addr;
    size_t size, return_length;

    static const u16string_view fn = u"\\dev\\framebuffer";

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtOpenSection(&sect, SECTION_MAP_READ | SECTION_MAP_WRITE, &oa);
    if (!success(status)) {
        print("NtOpenSection returned {}\n", status);
        return {};
    }

    status = NtQuerySection(sect, SECTION_INFORMATION_CLASS::MoxnoxFramebufferInformation,
                            &fb_info, sizeof(fb_info), &return_length);
    if (!success(status)) {
        print("NtQuerySection returned {}\n", status);
        NtClose(sect);
        return {};
    }

    addr = nullptr;
    size = 0;

    status = NtMapViewOfSection(sect, NtCurrentProcess(), &addr, 0, 0, nullptr,
                                &size, SECTION_INHERIT::ViewUnmap, 0,
                                PAGE_READWRITE);
    if (!success(status)) {
        print("NtMapViewOfSection returned {}\n", status);
        NtClose(sect);
        return {};
    }

    // FIXME - unmap this at end of program

    NtClose(sect);

    return span((uint32_t*)addr, size / sizeof(uint32_t));
}

static span<const uint8_t> load_font(u16string_view fn) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    HANDLE f, sect;
    FILE_STANDARD_INFORMATION fsi;
    void* addr;
    size_t size;

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&f, FILE_READ_DATA, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN,
                          0, nullptr, 0);
    if (!success(status)) {
        print("NtCreateFile returned {}\n", status);
        return {};
    }

    status = NtQueryInformationFile(f, &iosb, &fsi, sizeof(fsi),
                                    FILE_INFORMATION_CLASS::StandardInformation);
    if (!success(status)) {
        print("NtQueryInformationFile returned {}\n", status);
        NtClose(f);
        return {};
    }

    status = NtCreateSection(&sect, SECTION_MAP_READ, nullptr, nullptr,
                             PAGE_READONLY, SEC_COMMIT, f);
    if (!success(status)) {
        print("NtCreateSection returned {}\n", status);
        NtClose(f);
        return {};
    }

    NtClose(f);

    addr = nullptr;
    size = 0;

    // FIXME - unmap this at end of program

    status = NtMapViewOfSection(sect, NtCurrentProcess(), &addr, 0, 0, nullptr,
                                &size, SECTION_INHERIT::ViewUnmap, 0, PAGE_READONLY);
    if (!success(status)) {
        print("NtMapViewOfSection returned {}\n", status);
        NtClose(sect);
        return {};
    }

    NtClose(sect);

    return span((uint8_t*)addr, (size_t)fsi.EndOfFile.QuadPart);
}

static glyph_info* load_glyph(font_info& font, FT_UInt glyph) {
    auto* tree = find_rbtree_node(font.glyphs, glyph);
    if (tree)
        return &rbtree_containing_record<glyph_info>(tree);

    auto error = FT_Load_Glyph(font.face, glyph, FT_LOAD_RENDER | FT_RENDER_MODE_MONO);
    if (error) {
        print("FT_Load_Glyph returned {:x}\n", error);
        return nullptr;
    }

    auto gi = new glyph_info;
    if (!gi)
        return nullptr;

    gi->tree.key = glyph;
    gi->bitmap_left = font.face->glyph->bitmap_left;
    gi->bitmap_top = font.face->glyph->bitmap_top;

    FT_Bitmap_Init(&gi->bitmap);

    error = FT_Bitmap_Copy(ft, &font.face->glyph->bitmap, &gi->bitmap);
    if (error) {
        FT_Bitmap_Done(ft, &gi->bitmap);
        delete gi;
    }

    insert_into_rbtree(font.glyphs, gi->tree);

    return gi;
}

static void draw_ft_char(FT_UInt glyph, unsigned int fb_x, unsigned int fb_y) {
    auto gi = load_glyph(font, glyph);
    if (!gi)
        return;

    auto& bitmap = gi->bitmap;
    auto buf = bitmap.buffer;

    auto width = (unsigned int)bitmap.width;
    if ((int)fb_x + gi->bitmap_left + (int)width > (int)fb_info.width)
        width = (unsigned int)((int)fb_info.width - (int)fb_x - gi->bitmap_left);

    int y = (int)fb_y - gi->bitmap_top;
    int bottom = y + (int)bitmap.rows;

    if (y < 0) {
        buf += bitmap.width * ((unsigned int)gi->bitmap_top - fb_y);
        y = 0;
    }

    if (y >= (int)fb_info.height)
        return;

    if (bottom > (int)fb_info.height)
        bottom = (int)fb_info.height;

    auto bg = (uint32_t)0x000000; // FIXME
    auto fg = (uint32_t)0xffffff; // FIXME

    uint32_t bg_colour_r = bg >> 16;
    uint32_t fg_colour_r = fg >> 16;
    uint32_t bg_colour_g = (bg & 0xff00) >> 8;
    uint32_t fg_colour_g = (fg & 0xff00) >> 8;
    uint32_t bg_colour_b = bg & 0xff;
    uint32_t fg_colour_b = fg & 0xff;

    auto fb_pixels_per_scan_line = fb_info.width; // FIXME

    auto base = framebuffer.data() + ((uint32_t)y * fb_pixels_per_scan_line) + fb_x + gi->bitmap_left;

    while (y < bottom) {
        for (unsigned int x = 0; x < width; x++) {
            if (*buf == 255)
                base[x] = fg;
            else if (*buf != 0) {
                auto r = (uint8_t)(((bg_colour_r * (255 - (uint32_t)*buf)) + (fg_colour_r * (uint32_t)*buf)) / 255);
                auto g = (uint8_t)(((bg_colour_g * (255 - (uint32_t)*buf)) + (fg_colour_g * (uint32_t)*buf)) / 255);
                auto b = (uint8_t)(((bg_colour_b * (255 - (uint32_t)*buf)) + (fg_colour_b * (uint32_t)*buf)) / 255);

                base[x] = (unsigned int)((r << 16) | (g << 8) | b);
            }

            buf++;
        }

        buf += bitmap.width - width;

        base += fb_pixels_per_scan_line;
        y++;
    }
}

static void test_print() {
    NTSTATUS status;
    IO_STATUS_BLOCK iosb;
    void* flush_addr;
    size_t flush_size;

    auto glyph = FT_Get_Char_Index(font.face, 'A');

    draw_ft_char(glyph, 50, 60);

    flush_addr = framebuffer.data();
    flush_size = framebuffer.size_bytes();

    status = NtFlushVirtualMemory(NtCurrentProcess(), &flush_addr, &flush_size,
                                  &iosb);
    if (!success(status)) {
        print("NtFlushVirtualMemory returned {}\n", status);
        return;
    }
}

static int _main() {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;
    char s[10];

    framebuffer = init_framebuffer();
    if (framebuffer.empty()) {
        print("init_framebuffer failed\n");
        return 1;
    }

    auto font = load_font(u"console.ttf");
    if (font.empty()) {
        print("load_font failed\n");
        return 1;
    }

    if (!init_font(font)) {
        print("init_font failed\n");
        return 1;
    }

    test_print();

    // FIXME

    // FIXME
    NtReadFile(peb->ProcessParameters->StandardInput, 0, nullptr,
               nullptr, &iosb, s, sizeof(s), nullptr, nullptr);

    return 0;
}

extern "C"
void startup() {
    NtTerminateThread(NtCurrentThread(), (NTSTATUS)_main());
}
