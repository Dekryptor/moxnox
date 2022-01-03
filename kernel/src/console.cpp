#include "kernel.h"
#include "spinlock.h"
#include "unique_handle.h"
#include "objects.h"
#include <span>
#include <cstring>
#include <bit>
#include <ft2build.h>
#include <freetype/freetype.h>

using namespace std;

static constexpr unsigned int COLS = 80;
static constexpr unsigned int ROWS = 25;

enum class console_colour : uint8_t {
    black = 0,
    blue = 1,
    green = 2,
    cyan = 3,
    red = 4,
    magenta = 5,
    brown = 6,
    light_grey = 7,
    dark_grey = 8,
    light_blue = 9,
    light_green = 10,
    light_cyan = 11,
    light_red = 12,
    light_magenta = 13,
    yellow = 14,
    white = 15
};

static const enum console_colour fg_colour = console_colour::light_grey;
static const enum console_colour bg_colour = console_colour::black;

static const uint64_t CONSOLE_PA = 0xb8000;

static span<uint16_t> text_console;
static uint8_t cursor_x = 0, cursor_y = 0;

static void early_print(string_view s);
decltype(&safe_print) print = early_print;

static unique_handle console_pipe;
static const unsigned int console_buffer_pages = 4;

static span<uint32_t> framebuffer;
static uint32_t fb_width, fb_height, fb_pixels_per_scan_line;
static FT_Library ft = nullptr;
static FT_Face face = nullptr;
static unsigned int font_height;

size_t strlen(const char* s) {
    size_t i = 0;

    while (s[i] != 0) {
        i++;
    }

    return i;
}

static void text_console_putc(uint8_t x, uint8_t y, uint8_t c) {
    text_console[(y * COLS) + x] = (uint16_t)(((uint16_t)bg_colour << 12) | ((uint16_t)fg_colour << 8) | c);
}

static void text_console_overflow() {
    memcpy(text_console.data(), &text_console[COLS], (ROWS - 1) * COLS * sizeof(uint16_t));
    memset(text_console.data() + ((ROWS - 1) * COLS), 0, COLS * sizeof(uint16_t));
    cursor_y = ROWS - 1;
}

static void text_console_print(string_view s) {
    for (auto c : s) {
        if (c == '\n') {
            cursor_x = 0;
            cursor_y++;

            if (cursor_y == ROWS)
                text_console_overflow();
        } else {
            text_console_putc(cursor_x, cursor_y, c);

            cursor_x++;

            if (cursor_x == COLS) {
                cursor_x = 0;
                cursor_y++;

                if (cursor_y == ROWS)
                    text_console_overflow();
            }
        }
    }
}

static void early_print(string_view s) {
    if (framebuffer.empty()) {
        text_console_print(s);
        return;
    }

    // FIXME
}

void safe_print(string_view s) {
    write(console_pipe.get(), s);

    if (com1_pipe_handle)
        write(com1_pipe_handle.get(), s);
}

void print_hex(uint64_t num, unsigned int padding) {
    char s[17];
    span sp(s, sizeof(s));

    sp = sp_itoa_hex(sp, num, padding);

    print(string_view(s, sp.data() - s));
}

bool init_console(void* pa, uint32_t width, uint32_t height, uint32_t pixels_per_scan_line) {
    if (!pa) {
        auto va = allocate_va(1);
        if (!va) {
            // FIXME - exception
            print("allocate_va failed\n");
            return false;
        }

        text_console = span((uint16_t*)va, COLS * ROWS);

        if (!map_page(CONSOLE_PA, (uintptr_t)text_console.data(), mapping_type::read_write))
            return false;

        // clear console
        memset(text_console.data(), 0, text_console.size_bytes());
    } else {
        uint32_t size = pixels_per_scan_line * height * sizeof(uint32_t);

        if (size == 0) {
            print("Framebuffer size was 0.\n");
            return false;
        }

        auto pages = bytes_to_pages(size);
        auto va = allocate_va(pages);
        if (!va) {
            print("allocate_va failed\n");
            return false;
        }

        framebuffer = span((uint32_t*)va, pixels_per_scan_line * height);

        for (unsigned int i = 0; i < pages; i++) {
            if (!map_page((uintptr_t)pa + (i << PAGE_SHIFT), va + (i << PAGE_SHIFT), mapping_type::no_cache)) { // FIXME - should be write-combine (but needs MTRR)
                print("map_page failed\n");
                return false;
            }
        }

        fb_width = width;
        fb_height = height;
        fb_pixels_per_scan_line = pixels_per_scan_line;

        memset(framebuffer.data(), 0, framebuffer.size_bytes());
    }

    print("Hello\nworld\n");

    return true;
}

static void console_event_recv() {
    do {
        uint8_t s[255];
        wait_for_object(console_pipe.get());

        auto ret = read(console_pipe.get(), s);

        // FIXME - watch out for partial UTF-8 characters, partial escape sequences, etc.

        if (ret != 0)
            early_print(string_view((char*)s, ret));
    } while (true);
}

bool init_console_pipe() {
    console_pipe = create_pipe(console_buffer_pages);
    if (!console_pipe) {
        print("create_pipe failed\n");
        return false;
    }

    if (!name_object(console_pipe.get(), "tty")) {
        print("name_object failed\n");
        return false;
    }

    if (!add_kernel_thread(console_event_recv)) {
        print("add_kernel_thread failed\n");
        return false;
    }

    return true;
}

static void* ft_alloc(FT_Memory, long size) {
    return malloc(size);
}

static void* ft_realloc(FT_Memory, long cur_size, long new_size, void* block) {
    void* ret;

    ret = malloc(new_size);
    if (!ret)
        return nullptr;

    memcpy(ret, block, cur_size < new_size ? new_size : cur_size);

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

static void draw_ft_char(char32_t cp, unsigned int fb_x, unsigned int fb_y) {
    FT_Error error;

    // FIXME - colour
    // FIXME - fb_y should be baseline?

    if (fb_y >= fb_height)
        return;

    error = FT_Load_Char(face, cp, FT_LOAD_RENDER | FT_RENDER_MODE_MONO);
    if (error)
        return; // FIXME - if not found, print Unicode replacement character?

    auto& bitmap = face->glyph->bitmap;
    auto buf = bitmap.buffer;
    auto base = framebuffer.data() + (fb_y * fb_pixels_per_scan_line) + fb_x;

    auto width = bitmap.width;
    if (fb_x + face->glyph->bitmap_left + width > fb_width)
        width = fb_width - fb_x - face->glyph->bitmap_left;

    unsigned int y = fb_y;

    if ((int)fb_y < face->glyph->bitmap_top) {
        y += face->glyph->bitmap_top - fb_y;
        buf += bitmap.width * (face->glyph->bitmap_top - fb_y);
    }

    while (y < min(fb_y + bitmap.rows, fb_height)) {
        if (y - face->glyph->bitmap_top >= fb_height)
            break;

        for (unsigned int x = 0; x < width; x++) {
            if (*buf != 0)
                base[x] = (*buf << 16) | (*buf << 8) | *buf;

            buf++;
        }

        buf += bitmap.width - width;

        base += fb_pixels_per_scan_line;
        y++;
    }
}

bool init_fb_console() {
    FT_Error error;

    error = FT_Init_FreeType(&ft);
    if (error) {
        print("FT_Init_FreeType failed (");
        print_hex(error);
        print(")\n");

        return false;
    }

    // load console.ttf to memory

    auto file = open_file("/console.ttf");
    if (!file) {
        print("open_file failed\n");
        return false;
    }

    optional<uint64_t> file_size;

    {
        auto objrw = deref_handle(file.get());
        if (!objrw) {
            // FIXME - exception
            print("No such handle\n");
            return false;
        }

        auto& obj = objrw->get();

        visit_obj(obj, [&](auto&& o) {
            if constexpr (is_same_v<remove_reference_t<decltype(o)>, inode>)
                file_size = o.file_size;

            o.release();
        });
    }

    if (!file_size) {
        print("Could not get size of font file\n");
        return false;
    }

    if (file_size == 0) {
        print("Font file was empty\n");
        return false;
    }

    auto font_data = malloc(*file_size);
    if (!font_data) {
        print("malloc failed\n");
        return false;
    }

    if (read(file.get(), span((uint8_t*)font_data, *file_size), 0) < *file_size) {
        free(font_data);
        print("read failed\n");
        return false;
    }

    file.reset();

    error = FT_New_Memory_Face(ft, (uint8_t*)font_data, (FT_Long)*file_size, 0, &face);
    if (error) {
        free(font_data);

        print("FT_New_Memory_Face failed (");
        print_hex(error);
        print(")\n");

        return false;
    }

    free(font_data);

    // FIXME - font size
    error = FT_Set_Char_Size(face, 24 * 64, 0, 72, 0); // FIXME - get DPI from EDID
    if (error) {
        print("FT_Set_Char_Size failed (");
        print_hex(error);
        print(")\n");

        return false;
    }

    font_height = face->size->metrics.height / 64;

    // FIXME - work out monospace width and height of framebuffer console

    // FIXME
    draw_ft_char('A', 0, 50);
    draw_ft_char('B', 2552, 100);
    draw_ft_char('C', 130, 1592);

    return true;
}
