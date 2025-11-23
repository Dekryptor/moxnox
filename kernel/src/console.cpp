module;

#include <span>
#include <string_view>
#include <span>
#include <cstring>
#include <bit>
#include <mutex>
#include <console.h>
#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftbitmap.h>

export module console;

#ifndef __aarch64__
import inout;
#endif

import nt;
import print;
import mem;
import config;
import rbtree;
import visit;
import vt100;
import cp437;
import cmdline;
import thread;
import bsod;
import chardev;
import pipe;
import acpi;
import object_ref;
import scheduler;
import list;
import disable_interrupts;
import cpu_block;
import spinlock;
import loaderblock;
import framebuffer;
import handle;
import objects;

using namespace std;

export unsigned int console_cols; // FIXME

#ifndef __aarch64__
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
#endif

export void early_print(string_view s);

export object_ref console_chardev;
static object_ref kmsg;
static const unsigned int console_buffer_pages = 4;

export object_ref com1_pipe_handle;

static FT_Library ft = nullptr;
static unsigned int font_width, font_height, font_descender;
static bool fb_console_started = false;
static bool show_cursor = true;

#ifndef __aarch64__
static bool text_console_started = false;;
#endif

spinlock early_print_lock;
char early_print_buf[65536];
size_t early_print_buf_size = 0;

static uint32_t console_output_mode = ENABLE_PROCESSED_OUTPUT;

// font.S
extern "C" void* console_font_data;
extern "C" uint64_t console_font_size;

static const auto console_font = span(&console_font_data, console_font_size);

struct glyph_info {
    rbtree_node<FT_UInt> tree;
    FT_Int bitmap_left;
    FT_Int bitmap_top;
    FT_Bitmap bitmap;
};

// FIXME - LRU for glyphs, and free these occasionally?
struct font_info {
    FT_Face face = nullptr;
    rbtree_node<FT_UInt>* glyphs = nullptr; // FIXME - destructor for this
};

static font_info face;

class console : public pipe {
public:
    console(unsigned int buffer_pages) : pipe(buffer_pages) { }

    nt_status flush(bool go_readonly) override;
};

struct kmsg_buf_page {
    list_entry le;
    uint64_t pa;
    uint16_t read_pos = 0;
    uint16_t write_pos = 0;
};

class kmsg_buf : public chardev {
public:
    kmsg_buf() {
        signalled = false;
    }

    nt_status read(span<uint8_t> s, uint64_t& read_bytes) override;
    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override;

    spinlock lock;
    list_entry pages = { &pages, &pages };
};

struct console_info {
    uint32_t bg;
    uint32_t fg;
    bool bold;
    FT_UInt glyph;
    bool changed;
};

static console_info* console;

static const uint32_t vt100_colours[] = {
    0x000000,
    0xb21818,
    0x18b218,
    0xb26818,
    0x1818b2,
    0xb218b2,
    0x18b2b2,
    0xb2b2b2,
};

static const uint32_t vt100_bright_colours[] = {
    0x686868,
    0xff5454,
    0x54ff54,
    0xffff54,
    0x5454ff,
    0xff54ff,
    0x54ffff,
    0xffffff,
};

#ifndef __aarch64__
static const enum console_colour vt100_colours_text[] = {
    console_colour::black,
    console_colour::red,
    console_colour::green,
    console_colour::brown,
    console_colour::blue,
    console_colour::magenta,
    console_colour::cyan,
    console_colour::light_grey,
};

static const enum console_colour vt100_bright_colours_text[] = {
    console_colour::dark_grey,
    console_colour::light_red,
    console_colour::light_green,
    console_colour::yellow,
    console_colour::light_blue,
    console_colour::light_magenta,
    console_colour::light_cyan,
    console_colour::white,
};
#endif

static const unsigned int DEFAULT_BG_COLOUR_INDEX = 0;
static const unsigned int DEFAULT_FG_COLOUR_INDEX = 7;
static const uint32_t DEFAULT_BG_COLOUR = vt100_colours[DEFAULT_BG_COLOUR_INDEX];
static const uint32_t DEFAULT_FG_COLOUR = vt100_colours[DEFAULT_FG_COLOUR_INDEX];
static uint32_t bg_colour_index = 0, fg_colour_index = 7;
static bool bg_bright = false, fg_bright = false, bold = false;
static uint32_t bg_colour = DEFAULT_BG_COLOUR, fg_colour = DEFAULT_FG_COLOUR;
#ifndef __aarch64__
static const auto DEFAULT_BG_COLOUR_TEXT = console_colour::black;
static const auto DEFAULT_FG_COLOUR_TEXT = console_colour::light_grey;
static enum console_colour bg_colour_text = DEFAULT_BG_COLOUR_TEXT;
static enum console_colour fg_colour_text = DEFAULT_FG_COLOUR_TEXT;
#endif

static const size_t EDID_SIZE = 128;
export uint8_t edid[EDID_SIZE];
export bool have_edid;

export enum class console_type {
#ifndef __aarch64__
    text,
#endif
    boot_framebuffer,
    other_framebuffer,
    other
};

export enum console_type console_type;

struct console_fb {
    void console_overflow(unsigned int rows = 1);
    static void handle_sgr_part_num(unsigned int num);
    void print_chars(span<const char32_t> s, bool no_move);
    void cursor_changed();
    void scroll_down(unsigned int rows);
    static void enable_cursor(bool enable);
    void move_cursor() { }

    unsigned int cursor_x, cursor_y;
    unsigned int console_rows, console_cols;
};

struct console_text {
    void console_overflow(unsigned int rows = 1);
    static void handle_sgr_part_num(unsigned int num);
    void print_chars(span<const char32_t> s, bool no_move);
    void cursor_changed() { }
    void scroll_down(unsigned int rows);
    static void enable_cursor(bool enable);
    void move_cursor();

    unsigned int cursor_x, cursor_y;
    unsigned int console_rows, console_cols;
};

static console_fb console_fb_inst;
static console_text console_text_inst;

#ifndef __aarch64__
static constexpr uint16_t text_console_val(uint8_t c, enum console_colour bg,
                                           enum console_colour fg) {
    return (uint16_t)(((uint16_t)bg << 12) | ((uint16_t)fg << 8) | c);
}

static void text_console_putc(uint8_t x, uint8_t y, char32_t cp,
                              enum console_colour bg, enum console_colour fg) {
    auto c = cp437(cp);

    text_console[(y * console_text_inst.console_cols) + x] = text_console_val((uint8_t)c, bg, fg);
}

void console_text::move_cursor() {
    auto pos = (uint16_t)(((uint8_t)cursor_y * console_cols) + (uint8_t)cursor_x);

    {
        lock_guard lock(cursor_lock);

        outb(0x3d4, 0xf);
        outb(0x3d5, (uint8_t)(pos & 0xff));
        outb(0x3d4, 0xe);
        outb(0x3d5, (uint8_t)((pos >> 8) & 0xff));
    }
}

void console_text::console_overflow(unsigned int rows) {
    memcpy(text_console.data(), &text_console[console_cols * rows],
            (console_rows - rows) * console_cols * sizeof(uint16_t));

    for (uint8_t y = 0; y < rows; y++) {
        for (uint8_t x = 0; x < console_cols; x++) {
            text_console_putc(x, (uint8_t)(console_rows - rows + y), ' ',
                                bg_colour_text, fg_colour_text);
        }
    }
}

void console_text::print_chars(span<const char32_t> s, bool no_move) {
    for (auto c : s) {
        if (c == '\n') {
            if (!no_move) {
                cursor_x = 0;
                cursor_y++;

                if (cursor_y == console_rows) {
                    console_overflow();
                    cursor_y = console_rows - 1;
                }
            }
        } else if (c == '\b') {
            if (cursor_x == 0 && cursor_y == 0)
                continue;

            if (cursor_x > 0) { // FIXME
                cursor_x--;

                if (console_output_mode & ENABLE_PROCESSED_OUTPUT) {
                    text_console_putc((uint8_t)cursor_x, (uint8_t)cursor_y, ' ',
                                    bg_colour_text, fg_colour_text);
                }
            }
        } else {
            if (c < 0x20)
                continue;

            text_console_putc((uint8_t)cursor_x, (uint8_t)cursor_y, c,
                              bg_colour_text, fg_colour_text);

            if (!no_move) {
                cursor_x++;

                if (cursor_x == console_cols) {
                    cursor_x = 0;
                    cursor_y++;

                    if (cursor_y == console_rows) {
                        console_overflow();
                        cursor_y = console_rows - 1;
                    }
                }
            }
        }
    }
}

static void recalc_colours_text(bool fg) {
    bool bright = fg ? fg_bright : bg_bright;
    unsigned int index = fg ? fg_colour_index : bg_colour_index;
    enum console_colour* colour = fg ? &fg_colour_text : &bg_colour_text;

    if (bright || (fg && bold))
        *colour = vt100_bright_colours_text[index];
    else
        *colour = vt100_colours_text[index];
}

void console_text::handle_sgr_part_num(unsigned int num) {
    switch (num) {
        case 0: // reset
            fg_colour_index = DEFAULT_FG_COLOUR_INDEX;
            fg_bright = false;
            fg_colour_text = DEFAULT_FG_COLOUR_TEXT;

            bg_colour_index = DEFAULT_BG_COLOUR_INDEX;
            bg_bright = false;
            bg_colour_text = DEFAULT_BG_COLOUR_TEXT;

            bold = false;
            break;

        case 1:
            bold = true;
            recalc_colours_text(true);
            break;

        // FIXME - 5 - blink
        // FIXME - 7 - reverse video

        case 22:
            bold = false;
            recalc_colours_text(false);
            break;

        // FIXME - 25 - not blinking

        case 30:
        case 31:
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
            fg_colour_index = num - 30;
            fg_bright = false;
            recalc_colours_text(true);
            break;

        case 39:
            fg_colour_index = DEFAULT_FG_COLOUR_INDEX;
            fg_bright = false;
            fg_colour_text = DEFAULT_FG_COLOUR_TEXT;
            break;

        case 40:
        case 41:
        case 42:
        case 43:
        case 44:
        case 45:
        case 46:
        case 47:
            bg_colour_index = num - 40;
            bg_bright = false;
            recalc_colours_text(false);
            break;

        case 49:
            bg_colour_index = DEFAULT_BG_COLOUR_INDEX;
            bg_colour_text = DEFAULT_BG_COLOUR_TEXT;
            bg_bright = false;
            break;

        case 90:
        case 91:
        case 92:
        case 93:
        case 94:
        case 95:
        case 96:
        case 97:
            fg_colour_index = num - 90;
            fg_bright = true;
            recalc_colours_text(true);
            break;

        case 100:
        case 101:
        case 102:
        case 103:
        case 104:
        case 105:
        case 106:
        case 107:
            bg_colour_index = num - 100;
            bg_bright = true;
            recalc_colours_text(false);
            break;
    }
}

void console_text::enable_cursor(bool enabled) {
    if (enabled) {
        uint8_t cursor_start = 14, cursor_end = 15;

        outb(0x3d4, 0x0a);
        outb(0x3d5, (inb(0x3d5) & 0xc0) | cursor_start);

        outb(0x3d4, 0x0b);
        outb(0x3d5, (inb(0x3d5) & 0xe0) | cursor_end);
    } else {
        outb(0x3d4, 0xa);
        outb(0x3d5, 0x20);
    }
}

void console_text::scroll_down(unsigned int rows) {
    memmove(&text_console[console_cols * rows], text_console.data(),
            (console_rows - rows) * console_cols * sizeof(uint16_t));

    for (uint8_t y = 0; y < rows; y++) {
        for (uint8_t x = 0; x < console_cols; x++) {
            text_console_putc(x, y, ' ', bg_colour_text, fg_colour_text);
        }
    }
}
#endif

void console_fb::console_overflow(unsigned int rows) {
    memcpy(console, &console[console_cols * rows], (console_rows - rows) * console_cols * sizeof(console_info));

    auto c = &console[(console_rows - rows) * console_cols];

    auto glyph = FT_Get_Char_Index(face.face, ' ');

    for (unsigned int i = 0; i < console_cols * console_rows; i++) {
        console[i].changed = true;
    }

    for (unsigned int i = 0; i < console_cols * rows; i++) {
        c->bg = bg_colour;
        c->fg = fg_colour;
        c->glyph = glyph;
        c->bold = bold;
        c++;
    }
}

void console_fb::print_chars(span<const char32_t> s, bool no_move) {
    for (auto c : s) {
        if (c == '\n') {
            if (console) {
                auto glyph = FT_Get_Char_Index(face.face, ' ');

                for (unsigned int i = cursor_x; i < console_cols; i++) {
                    if (console[(cursor_y * console_cols) + i].glyph != glyph)
                        continue;

                    console[(cursor_y * console_cols) + i].glyph = glyph;
                    console[(cursor_y * console_cols) + i].changed = true;
                }
            }

            if (!no_move) {
                cursor_x = 0;
                cursor_y++;

                if (cursor_y == console_rows) {
                    console_overflow();
                    cursor_y = console_rows - 1;
                }
            }
        } else if (c == '\b') {
            if (cursor_x == 0 && cursor_y == 0)
                continue;

            if (cursor_x > 0)
                cursor_x--;
            else {
                cursor_x = console_cols - 1;
                cursor_y--;

                if (cursor_x != console_cols - 1)
                    cursor_x++;
            }

            if (console && console_output_mode & ENABLE_PROCESSED_OUTPUT) {
                auto glyph = FT_Get_Char_Index(face.face, ' ');

                if (console[(cursor_y * console_cols) + cursor_x].glyph != glyph) {
                    console[(cursor_y * console_cols) + cursor_x].glyph = glyph;
                    console[(cursor_y * console_cols) + cursor_x].changed = true;
                }
            }
        } else {
            if (c < 0x20)
                continue;

            if (console) {
                auto& p = console[(cursor_y * console_cols) + cursor_x];

                auto glyph = FT_Get_Char_Index(face.face, c);

                p.glyph = glyph;
                p.bg = bg_colour;
                p.fg = fg_colour;
                p.bold = bold;
                p.changed = true;
            }

            if (!no_move) {
                cursor_x++;

                if (cursor_x == console_cols) {
                    cursor_x = 0;
                    cursor_y++;

                    if (cursor_y == console_rows) {
                        console_overflow();
                        cursor_y = console_rows - 1;
                    }
                }
            }
        }
    }
}

void console_fb::cursor_changed() {
    if (show_cursor) {
        auto& p = console[(cursor_y * console_cols) + cursor_x];

        p.changed = true;
    }
}

static void recalc_colours(bool fg) {
    bool bright = fg ? fg_bright : bg_bright;
    unsigned int index = fg ? fg_colour_index : bg_colour_index;
    uint32_t* colour = fg ? &fg_colour : &bg_colour;

    if (bright || (fg && bold))
        *colour = vt100_bright_colours[index];
    else
        *colour = vt100_colours[index];
}

void console_fb::handle_sgr_part_num(unsigned int num) {
    switch (num) {
        case 0: // reset
            fg_colour_index = DEFAULT_FG_COLOUR_INDEX;
            fg_bright = false;
            fg_colour = DEFAULT_FG_COLOUR;

            bg_colour_index = DEFAULT_BG_COLOUR_INDEX;
            bg_bright = false;
            bg_colour = DEFAULT_BG_COLOUR;

            bold = false;
            break;

        case 1:
            bold = true;
            recalc_colours(true);
            break;

        // FIXME - 3 - italic
        // FIXME - 4 - underline
        // FIXME - 5 - blink
        // FIXME - 7 - reverse video
        // FIXME - 9 - strikethrough
        // FIXME - 21 - double underline

        case 22:
            bold = false;
            recalc_colours(false);
            break;

        // FIXME - 23 - not italic
        // FIXME - 24 - not underlined
        // FIXME - 25 - not blinking

        case 30:
        case 31:
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
            fg_colour_index = num - 30;
            fg_bright = false;
            recalc_colours(true);
            break;

        // FIXME - 38 - custom foreground colour

        case 39:
            fg_colour_index = DEFAULT_FG_COLOUR_INDEX;
            fg_bright = false;
            fg_colour = DEFAULT_FG_COLOUR;
            break;

        case 40:
        case 41:
        case 42:
        case 43:
        case 44:
        case 45:
        case 46:
        case 47:
            bg_colour_index = num - 40;
            bg_bright = false;
            recalc_colours(false);
            break;

        // FIXME - 48 - custom background colour

        case 49:
            bg_colour_index = DEFAULT_BG_COLOUR_INDEX;
            bg_colour = DEFAULT_BG_COLOUR;
            bg_bright = false;
            break;

        case 90:
        case 91:
        case 92:
        case 93:
        case 94:
        case 95:
        case 96:
        case 97:
            fg_colour_index = num - 90;
            fg_bright = true;
            recalc_colours(true);
            break;

        case 100:
        case 101:
        case 102:
        case 103:
        case 104:
        case 105:
        case 106:
        case 107:
            bg_colour_index = num - 100;
            bg_bright = true;
            recalc_colours(false);
            break;
    }
}

void console_fb::enable_cursor(bool enabled) {
    show_cursor = enabled;
}

void console_fb::scroll_down(unsigned int rows) {
    memmove(&console[console_cols * rows], console,
            (console_rows - rows) * console_cols * sizeof(console_info));

    auto c = &console[0];

    auto glyph = FT_Get_Char_Index(face.face, ' ');

    for (unsigned int i = 0; i < console_cols * console_rows; i++) {
        console[i].changed = true;
    }

    for (unsigned int i = 0; i < console_cols * rows; i++) {
        c->bg = bg_colour;
        c->fg = fg_colour;
        c->glyph = glyph;
        c->bold = bold;
        c++;
    }
}

static glyph_info* load_glyph(font_info& font, FT_UInt glyph) {
    auto* tree = find_rbtree_node(font.glyphs, glyph);
    if (tree)
        return &rbtree_containing_record<glyph_info>(tree);

    auto error = FT_Load_Glyph(font.face, glyph, FT_LOAD_RENDER | FT_RENDER_MODE_MONO);
    if (error) // FIXME - report somehow
        return nullptr;

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

static void draw_ft_char(const console_info& c, unsigned int fb_x, unsigned int fb_y,
                         bool is_cursor) {
    auto fb = console_type == console_type::boot_framebuffer ? shadow_framebuffer : framebuffer;

    auto gi = load_glyph(face, c.glyph);
    if (!gi)
        return;

    auto& bitmap = gi->bitmap;
    auto buf = bitmap.buffer;

    auto width = (unsigned int)bitmap.width;
    if ((int)fb_x + gi->bitmap_left + (int)width > (int)fb_width)
        width = (unsigned int)((int)fb_width - (int)fb_x - gi->bitmap_left);

    int y = (int)fb_y - gi->bitmap_top;
    int bottom = y + (int)bitmap.rows;

    if (y < 0) {
        buf += bitmap.width * ((unsigned int)gi->bitmap_top - fb_y);
        y = 0;
    }

    if (y >= (int)fb_height)
        return;

    if (bottom > (int)fb_height)
        bottom = (int)fb_height;

    auto bg = c.bg;
    auto fg = c.fg;

    if (is_cursor)
        swap(bg, fg);

    uint32_t bg_colour_r = bg >> 16;
    uint32_t fg_colour_r = fg >> 16;
    uint32_t bg_colour_g = (bg & 0xff00) >> 8;
    uint32_t fg_colour_g = (fg & 0xff00) >> 8;
    uint32_t bg_colour_b = bg & 0xff;
    uint32_t fg_colour_b = fg & 0xff;

    auto base = fb.data() + ((uint32_t)y * fb_pixels_per_scan_line) + fb_x + gi->bitmap_left;

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

static void rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, uint32_t col) {
    auto fb = console_type == console_type::boot_framebuffer ? shadow_framebuffer : framebuffer;
    auto dest = fb.data() + (fb_pixels_per_scan_line * y) + x;

    for (unsigned int i = 0; i < h; i++) {
        for (unsigned int j = 0; j < w; j++) {
            *dest = col;
            dest++;
        }

        dest += fb_pixels_per_scan_line - w;
    }
}

static void refresh_framebuffer() {
    auto* c = &console[0];

    for (unsigned int y = 0; y < console_fb_inst.console_rows; y++) {
        for (unsigned int x = 0; x < console_fb_inst.console_cols; x++) {
            if (!c->changed) {
                c++;
                continue;
            }

            bool is_cursor = show_cursor && x == console_fb_inst.cursor_x && y == console_fb_inst.cursor_y;

            rect(x * font_width, y * font_height, font_width, font_height, is_cursor ? c->fg : c->bg);
            draw_ft_char(*c, x * font_width, ((y + 1) * font_height) - font_descender, is_cursor);

            // don't leave black lines at edge of screen

            if (x == console_fb_inst.console_cols - 1 && font_width * console_fb_inst.console_cols < fb_width) {
                rect((x + 1) * font_width, y * font_height, fb_width - (font_width * console_fb_inst.console_cols),
                     font_height, c->bg);
            }

            if (y == console_fb_inst.console_rows - 1 && font_height * console_fb_inst.console_rows < fb_height) {
                rect(x * font_width, (y + 1) * font_height, font_width,
                     fb_height - (font_height * console_fb_inst.console_rows), c->bg);

                if (x == console_fb_inst.console_cols - 1 && font_width * console_fb_inst.console_cols < fb_width) {
                    rect((x + 1) * font_width, (y + 1) * font_height,
                         fb_width - (font_width * console_fb_inst.console_cols),
                         fb_height - (font_height * console_fb_inst.console_rows), c->bg);
                }
            }

            c++;
        }
    }
}

static void shadow_blit(unsigned int x, unsigned int y, unsigned int width,
                        unsigned int height) {
    for (unsigned int i = 0; i < height; i++) {
        memcpy(framebuffer.data() + (fb_pixels_per_scan_line * y) + x,
               shadow_framebuffer.data() + (fb_pixels_per_scan_line * y) + x,
               width * sizeof(uint32_t));
        y++;
    }
}

static void copy_shadow_framebuffer() {
    auto* c = &console[0];

    for (unsigned int y = 0; y < console_fb_inst.console_rows; y++) {
        for (unsigned int x = 0; x < console_fb_inst.console_cols; x++) {
            if (!c->changed) {
                c++;
                continue;
            }

            unsigned int run =  1;

            c->changed = false;
            c++;

            for (unsigned int i = x + 1; i < console_fb_inst.console_rows; i++) {
                if (!c->changed)
                    break;

                run++;
                c->changed = false;
                c++;
            }

            shadow_blit(x * font_width, y * font_height, run * font_width,
                        font_height);
            x += run - 1;
        }
    }
}

template<bool no_lock>
void add_to_early_print_buf(string_view s) {
    auto func = [&]() {
        if (early_print_buf_size == sizeof(early_print_buf))
            return;

        auto to_copy = min(s.size(), sizeof(early_print_buf) - early_print_buf_size);

        memcpy(early_print_buf + early_print_buf_size, s.data(), to_copy);

        early_print_buf_size += to_copy;
    };

    if constexpr (no_lock)
        func();
    else {
        lock_guard lock(early_print_lock);
        func();
    }
}

export void very_early_print(string_view s) {
    add_to_early_print_buf<true>(s);
}

void early_print(string_view s) {
#ifndef __aarch64__
    if (console_type == console_type::text) {
        if (!text_console_started) {
            add_to_early_print_buf<false>(s);
            return;
        }

        do_print(console_text_inst, s);
        return;
    }
#endif

    if (!fb_console_started) {
        add_to_early_print_buf<false>(s);
        return;
    }

    disable_preempt();

    do_print(console_fb_inst, s);

    refresh_framebuffer();

    if (flush_framebuffer)
        flush_framebuffer(0, 0, fb_width, fb_height);
    else
        copy_shadow_framebuffer();

    enable_preempt();
}

export void safe_print(string_view s) {
    uint64_t written_bytes;

    auto buf = span((uint8_t*)s.data(), s.size());

    // FIXME - how can we print errors if write fails?

    object_write_without_offset(console_chardev.get(), buf, written_bytes);

    object_write_without_offset(kmsg.get(), buf, written_bytes);

    // FIXME - this should be hidden behind a cmdline option
    if (console_type != console_type::other && com1_pipe_handle)
        object_write_without_offset(com1_pipe_handle.get(), buf, written_bytes);
}

static void console_event_recv() {
    auto buf_size = console_buffer_pages << PAGE_SHIFT;

    auto buf = (uint8_t*)vmalloc(buf_size);
    if (!buf)
        return;

    do {
        uint64_t read_bytes;

        wait_for_object(console_chardev.get(), false);

        auto status = static_cast<class console&>(console_chardev.get()).read(span(buf, buf_size), read_bytes);

        // FIXME - watch out for partial UTF-8 characters, partial escape sequences, etc.

        if (success(status) && read_bytes != 0)
            early_print(string_view((char*)buf, read_bytes));
    } while (true);

    vfree(buf, buf_size);
}

static void flush_early_print() {
    safe_print(string_view(early_print_buf, early_print_buf_size));
}

static bool create_kmsg() {
    auto pptr = new kmsg_buf;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    kmsg = object_ref{*pptr};

    auto status = name_object(kmsg.get(), "kmsg");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    return true;
}

#ifndef __aarch64__
export bool init_console(const loader_block_framebuffer& fb,
                         const loader_block_text_console& tc)
#else
export bool init_console(const loader_block_framebuffer& fb)
#endif
{
    string_view console_opt;

    loop_through_cmdline([&](string_view s) {
        static const string_view prefix = "console=";

        if (s.size() >= prefix.size() && string_view(s.data(), prefix.size()) == prefix) {
            console_opt = string_view(s.data() + prefix.size(), s.size() - prefix.size());
            return false;
        }

        return true;
    });

    if (!console_opt.empty()) {
        static const char prefix[] = "/dev/";

        // FIXME - these BSODs won't actually get displayed; do we need to initialize the FB in this case?

        if (console_opt.size() < sizeof(prefix) - 1 || memcmp(prefix, console_opt.data(), sizeof(prefix) - 1))
            bsod_format("unable to parse console option {}", console_opt);

        auto sv = string_view(console_opt.data() + sizeof(prefix) - 1,
                              console_opt.size() - sizeof(prefix) + 1);

        auto status = open_object(sv, console_chardev);
        if (!success(status))
            bsod_format("open_object returned {} trying to open console {}", status, console_opt);

        if (console_chardev.get().type != object_type::chardev)
            bsod_format("console option {} did not point to a character device", console_opt);

        console_text_inst.console_cols = 80; // FIXME
        console_text_inst.console_rows = 25; // FIXME
        console_cols = console_text_inst.console_cols;

        console_type = console_type::other;

        if (!create_kmsg())
            return false;

        flush_early_print();

        return true;
    }

#ifndef __aarch64__
    if (console_type == console_type::text) {
        console_text_inst.console_cols = tc.cols;
        console_text_inst.console_rows = tc.rows;
        console_cols = console_text_inst.console_cols;

        text_console = span((uint16_t*)phys_address(tc.phys_address),
                            console_text_inst.console_cols * console_text_inst.console_rows);

        // clear console (foreground colour needs to be initialized for cursor to show up)
        for (uint8_t y = 0; y < console_text_inst.console_rows; y++) {
            for (uint8_t x = 0; x < console_text_inst.console_cols; x++) {
                text_console_putc(x, y, ' ', console_colour::black, console_colour::light_grey);
            }
        }

        console_text_inst.cursor_x = 0;
        console_text_inst.cursor_y = 0;

        text_console_cols = console_text_inst.console_cols;
        text_console_rows = console_text_inst.console_rows;

        text_console_started = true;

        create_text_console_section();
    } else {
#endif
        if (!framebuffer.empty()) { // not using boot framebuffer
            console_type = console_type::other_framebuffer;
            create_other_framebuffer_section();
        } else {
            uint32_t size = fb.pixels_per_scan_line * fb.height * sizeof(uint32_t);

            if (size == 0) {
                print("Framebuffer size was 0.\n");
                return false;
            }

            auto num_pages = bytes_to_pages(size);
            auto va = allocate_va(num_pages);
            if (!va) {
                print("allocate_va failed\n");
                return false;
            }

            framebuffer = span((uint32_t*)va, fb.pixels_per_scan_line * fb.height);

            for (unsigned int i = 0; i < num_pages; i++) {
                if (!map_page((uintptr_t)fb.phys_address + (i << PAGE_SHIFT), va + (i << PAGE_SHIFT), mapping_type::write_combine)) {
                    print("map_page failed\n");
                    return false;
                }
            }

            auto ptr = (uint32_t*)vmalloc(fb.pixels_per_scan_line * fb.height * sizeof(uint32_t));
            if (!ptr) {
                print("vmalloc failed\n");
                return false;
            }

            shadow_framebuffer = span(ptr, fb.pixels_per_scan_line * fb.height);

            fb_width = fb.width;
            fb_height = fb.height;
            fb_pixels_per_scan_line = fb.pixels_per_scan_line;

            memset(framebuffer.data(), 0, framebuffer.size_bytes());
            memset(shadow_framebuffer.data(), 0, shadow_framebuffer.size_bytes());

            create_boot_framebuffer_section();
        }
#ifndef __aarch64__
    }
#endif

    {
        auto pptr = new class console(console_buffer_pages);
        if (!pptr) {
            print("new failed\n");
            return false;
        }

        console_chardev = object_ref{*pptr};

        auto status = name_object(console_chardev.get(), "tty");
        if (!success(status)) {
            print("name_object returned {}\n", status);
            return false;
        }
    }

    if (!create_kmsg())
        return false;

    if (framebuffer.empty()) { // gets started later if we have framebuffer
        if (!add_kernel_thread(console_event_recv)) {
            print("add_kernel_thread failed\n");
            return false;
        }
    }

    return true;
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

export bool init_fb_console() {
    FT_Error error;

    // FIXME - allow setting on cmdline
    static const unsigned int font_size = 12;

    error = FT_Init_FreeType(&ft);
    if (error) {
        print("FT_Init_FreeType failed ({:x})\n", error);
        return false;
    }

    if (have_edid && edid[21] != 0) {
        uint8_t screen_x_cm = edid[21];

        fb_dpi = (fb_width * 254) / ((uint16_t)screen_x_cm * 100);
    } else
        fb_dpi = 72;

    error = FT_New_Memory_Face(ft, (uint8_t*)&console_font_data, (FT_Long)console_font_size,
                               0, &face.face);
    if (error) {
        print("FT_New_Memory_Face failed ({:x})\n", error);
        return false;
    }

    error = FT_Set_Char_Size(face.face, font_size * 64, 0, fb_dpi, 0);
    if (error) {
        print("FT_Set_Char_Size failed ({:x})\n", error);
        return false;
    }

    font_width = (unsigned int)(face.face->size->metrics.max_advance / 64);
    font_height = (unsigned int)(face.face->size->metrics.height / 64);
    font_descender = (unsigned int)(-face.face->size->metrics.descender / 64);

    console_fb_inst.console_cols = fb_width / font_width;
    console_fb_inst.console_rows = fb_height / font_height;
    console_cols = console_fb_inst.console_cols;

    console_fb_inst.cursor_x = 0;
    console_fb_inst.cursor_y = 0;

    console = (console_info*)vmalloc(console_fb_inst.console_cols * console_fb_inst.console_rows * sizeof(console_info));
    if (!console) {
        print("out of memory\n");
        return false;
    }

    auto glyph = FT_Get_Char_Index(face.face, ' ');

    for (unsigned int i = 0; i < console_fb_inst.console_cols * console_fb_inst.console_rows; i++) {
        console[i].bg = DEFAULT_BG_COLOUR;
        console[i].fg = DEFAULT_FG_COLOUR;
        console[i].bold = false;
        console[i].glyph = glyph;
        console[i].changed = false;
    }

    if (!add_kernel_thread(console_event_recv)) {
        print("add_kernel_thread failed\n");
        return false;
    }

    fb_console_started = true;

    flush_early_print();

    return true;
}

export void copy_edid(loader_block* lb) {
    memcpy(edid, (void*)lb->edid, EDID_SIZE);
    have_edid = true;
}

nt_status console::flush(bool go_readonly) {
    if (go_readonly)
        readonly = true;

    sync_lock sl;
    lock_guard l(sl);
    lock_guard l2(lock);

    if (buf_unread == 0)
        return nt_status::SUCCESS;

    size_t buf_read_pos = buf_write_pos + (buf_pages << PAGE_SHIFT) - buf_unread;

    while (buf_read_pos >= buf_pages << PAGE_SHIFT) {
        buf_read_pos -= buf_pages << PAGE_SHIFT;
    }

    if (buf_read_pos + buf_unread > buf_pages << PAGE_SHIFT) {
        auto first_bit = (buf_pages << PAGE_SHIFT) - buf_read_pos;

        early_print(string_view((char*)&buf[buf_read_pos], first_bit));

        buf_read_pos = 0;
        buf_unread -= first_bit;
    }

    early_print(string_view((char*)&buf[buf_read_pos], buf_unread));
    buf_unread = 0;

    signalled = false;

    if (!not_full_event.signalled)
        not_full_event.set_holding_sync_lock();

    return nt_status::SUCCESS;
}

nt_status kmsg_buf::read(span<uint8_t> s, uint64_t& read_bytes) {
    list_entry del_list = { &del_list, &del_list };

    {
        sync_lock sl;
        lock_guard lg1(sl);
        lock_guard lg2(lock);

        read_bytes = 0;

        while (!s.empty() && !is_list_empty(pages)) {
            auto& p = containing_record<kmsg_buf_page>(pages.next);
            auto page = (uint8_t*)phys_address(p.pa);
            auto to_copy = min(s.size(), (size_t)(p.write_pos - p.read_pos));

            if (to_copy == 0)
                break;

            memcpy(s.data(), page + p.read_pos, to_copy);

            p.read_pos += (uint16_t)to_copy;
            read_bytes += to_copy;

            s = span(s.data() + to_copy, s.size() - to_copy);

            if (p.read_pos < PAGE_SIZE)
                break;

            remove_entry_list(p.le);
            insert_tail_list(del_list, p.le);
        }

        if (!is_list_empty(pages)) {
            auto& p = containing_record<kmsg_buf_page>(pages.prev);

            if (p.write_pos == p.read_pos)
                signalled = false;
        }
    }

    while (!is_list_empty(del_list)) {
        auto& p = containing_record<kmsg_buf_page>(del_list.next);

        remove_entry_list(p.le);

        free_folio(p.pa);
        delete &p;
    }

    return nt_status::SUCCESS;
}

nt_status kmsg_buf::write(span<const uint8_t> s, uint64_t& written_bytes) {
    {
        lock_guard lg(lock);

        written_bytes = 0;

        if (!is_list_empty(pages) && containing_record<kmsg_buf_page>(pages.prev).write_pos < PAGE_SIZE) {
            auto& p = containing_record<kmsg_buf_page>(pages.prev);
            auto page = (uint8_t*)phys_address(p.pa);
            size_t to_copy = min(s.size(), PAGE_SIZE - p.write_pos);

            memcpy(page + p.write_pos, s.data(), to_copy);
            p.write_pos += (uint16_t)to_copy;

            written_bytes = s.size();

            s = span(s.data() + to_copy, s.size() - to_copy);
        }

        while (!s.empty()) {
            auto p = new kmsg_buf_page;
            if (!p) {
                print("new failed\n");
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            p->pa = allocate_folio(0);
            if (!p->pa) {
                print("allocate_folio failed\n");
                delete p;
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            insert_tail_list(pages, p->le);

            auto page = (uint8_t*)phys_address(p->pa);
            size_t to_copy = min(s.size(), PAGE_SIZE);

            memcpy(page, s.data(), to_copy);
            p->write_pos += (uint16_t)to_copy;

            written_bytes += s.size();

            s = span(s.data() + to_copy, s.size() - to_copy);
        }
    }

    if (written_bytes > 0) {
        sync_lock sl;
        lock_guard l(sl);

        if (!signalled) {
            signalled = true;

            wake_up_waiters(waiters);
        }
    }

    return nt_status::SUCCESS;
}
