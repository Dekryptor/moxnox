#include "kernel.h"
#include "spinlock.h"
#include "objects.h"
#include "handle.h"
#include "mem.h"
#include <span>
#include <cstring>
#include <bit>
#include <ft2build.h>
#include <freetype/freetype.h>
#include <console.h>

using namespace std;

static unsigned int console_cols, console_rows;

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

static span<uint16_t> text_console;
static unsigned int cursor_x = 0, cursor_y = 0;

static void early_print(string_view s);
decltype(&safe_print) _printk = early_print;

static object_ref console_pipe;
static const unsigned int console_buffer_pages = 4;

static span<uint32_t> framebuffer, shadow_framebuffer;
static uint32_t fb_width, fb_height, fb_pixels_per_scan_line;
static FT_Library ft = nullptr;
static FT_Face face = nullptr, face_bold = nullptr;
static unsigned int font_width, font_height, font_descender;
static unsigned int fb_console_rows, fb_console_cols;
static bool fb_console_started = false;
static uint8_t utf8_buf[5];
static unsigned int utf8_chars = 0;
static spinlock cursor_lock, bsod_lock;
static char assertion_buffer[1024];
static char escape_buffer[50];
static size_t escape_buffer_len = 0;

static uint32_t console_output_mode = ENABLE_PROCESSED_OUTPUT;

// font.S
extern void* console_font_data;
extern void* console_font_bold_data;
extern uint64_t console_font_size;
extern uint64_t console_font_bold_size;

static const auto console_font = span(&console_font_data, console_font_size);
static const auto console_font_bold = span(&console_font_bold_data, console_font_bold_size);

struct console_info {
    uint32_t bg;
    uint32_t fg;
    bool bold;
    FT_UInt glyph;
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

static const uint32_t BSOD_BG_COLOUR = vt100_colours[4];
static const uint32_t BSOD_FG_COLOUR = vt100_bright_colours[7];
static const unsigned int DEFAULT_BG_COLOUR_INDEX = 0;
static const unsigned int DEFAULT_FG_COLOUR_INDEX = 7;
static const uint32_t DEFAULT_BG_COLOUR = vt100_colours[DEFAULT_BG_COLOUR_INDEX];
static const uint32_t DEFAULT_FG_COLOUR = vt100_colours[DEFAULT_FG_COLOUR_INDEX];
static uint32_t bg_colour_index = 0, fg_colour_index = 7;
static bool bg_bright = false, fg_bright = false, bold = false;
static uint32_t bg_colour = DEFAULT_BG_COLOUR, fg_colour = DEFAULT_FG_COLOUR;
static const auto DEFAULT_BG_COLOUR_TEXT = console_colour::black;
static const auto DEFAULT_FG_COLOUR_TEXT = console_colour::light_grey;
static enum console_colour bg_colour_text = DEFAULT_BG_COLOUR_TEXT;
static enum console_colour fg_colour_text = DEFAULT_FG_COLOUR_TEXT;

static const size_t EDID_SIZE = 128;
static uint8_t edid[EDID_SIZE];
static bool have_edid;

bool has_framebuffer;

static void fb_print2(char c);

size_t strlen(const char* s) {
    size_t i = 0;

    while (s[i] != 0) {
        i++;
    }

    return i;
}

static void text_console_putc(uint8_t x, uint8_t y, uint8_t c, enum console_colour bg,
                              enum console_colour fg) {
    text_console[(y * console_cols) + x] = (uint16_t)(((uint16_t)bg << 12) | ((uint16_t)fg << 8) | c);
}

static void text_console_overflow() {
    memcpy(text_console.data(), &text_console[console_cols], (console_rows - 1) * console_cols * sizeof(uint16_t));

    for (uint8_t x = 0; x < console_cols; x++) {
        text_console_putc(x, (uint8_t)(console_rows - 1), ' ', console_colour::black, console_colour::light_grey);
    }

    cursor_y = console_rows - 1;
}

__attribute((naked))
static void outb(uint16_t port, uint8_t val) {
    __asm __volatile(
        "mov al, %1\n"
        "\tmov dx, %0\n"
        "\tout dx, al\n"
        "\tret\n"
        :
        : "r" (port), "r" (val)
        : "al", "dx"
    );
}

static void text_console_move_cursor(uint8_t x, uint8_t y) {
    auto pos = (uint16_t)((y * console_cols) + x);

    {
        lock_guard lock(cursor_lock);

        outb(0x3d4, 0xf);
        outb(0x3d5, (uint8_t)(pos & 0xff));
        outb(0x3d4, 0xe);
        outb(0x3d5, (uint8_t)((pos >> 8) & 0xff));
    }
}

static void text_console_print2(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;

        if (cursor_y == console_rows)
            text_console_overflow();
    } else if (c == '\b') {
        if (cursor_x > 0) { // FIXME
            cursor_x--;

            if (console_output_mode & ENABLE_PROCESSED_OUTPUT) {
                text_console_putc((uint8_t)cursor_x, (uint8_t)cursor_y, ' ',
                                  bg_colour_text, fg_colour_text);
            }
        }
    } else {
        text_console_putc((uint8_t)cursor_x, (uint8_t)cursor_y, c,
                          bg_colour_text, fg_colour_text);

        cursor_x++;

        if (cursor_x == console_cols) {
            cursor_x = 0;
            cursor_y++;

            if (cursor_y == console_rows)
                text_console_overflow();
        }
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

static void recalc_colours_text(bool fg) {
    bool bright = fg ? fg_bright : bg_bright;
    unsigned int index = fg ? fg_colour_index : bg_colour_index;
    enum console_colour* colour = fg ? &fg_colour_text : &bg_colour_text;

    if (bright || (fg && bold))
        *colour = vt100_bright_colours_text[index];
    else
        *colour = vt100_colours_text[index];
}

template<bool FB>
static void handle_sgr_part(string_view s) {
    while (!s.empty() && s.front() == '0') {
        s = string_view(s.data() + 1, s.size() - 1);
    }

    unsigned int num = 0;

    for (auto c : s) {
        if (c < '0' || c > '9')
            return;

        num *= 10;
        num += c - '0';
    }

    if constexpr (FB) {
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
    } else {
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
}

template<bool FB>
static void handle_sgr(string_view s) {
    do {
        string_view p;

        auto sc = s.find(';');

        if (sc == string_view::npos)
            p = s;
        else
            p = string_view(s.data(), sc);

        handle_sgr_part<FB>(p);

        if (sc == string_view::npos)
            break;

        s = string_view(s.data() + sc + 1, s.size() - sc - 1);
    } while (true);
}

template<bool FB>
static void handle_escape_sequence(string_view s) {
    if (!(console_output_mode & ENABLE_PROCESSED_OUTPUT)) {
        for (auto c : s) {
            if constexpr (FB)
                fb_print2(c);
            else
                text_console_print2(c);
        }
        return;
    }

    if (s.size() >= 3 && s[0] == 0x1b && s[1] == '[' && s.back() == 'm')
        handle_sgr<FB>(string_view(s.data() + 2, s.size() - 3));
}

static void text_console_print(string_view s) {
    if (escape_buffer_len != 0) {
        if (escape_buffer_len == 1 && s[0] != '[') {
            text_console_print2(0x1b);
            escape_buffer_len = 0;
        } else {
            if (escape_buffer_len == 1) {
                escape_buffer[1] = '[';
                escape_buffer_len++;

                s = string_view(s.data() + 1, s.size() - 1);

                if (s.empty())
                    return;
            }

            while (!s.empty() && s.front() >= 0x30 && s.front() <= 0x3f) {
                if (escape_buffer_len == sizeof(escape_buffer)) {
                    for (size_t i = 0; i < escape_buffer_len; i++) {
                        text_console_print2(escape_buffer[i]);
                    }

                    escape_buffer_len = 0;
                    goto loop;
                }

                escape_buffer[escape_buffer_len] = s.front();
                escape_buffer_len++;
                s = string_view(s.data() + 1, s.size() - 1);
            }

            if (s.empty())
                return;

            if (s.front() >= 0x40 && s.front() <= 0x7e) {
                escape_buffer[escape_buffer_len] = s.front();
                escape_buffer_len++;

                s = string_view(s.data() + 1, s.size() - 1);

                handle_escape_sequence<false>(string_view(escape_buffer, escape_buffer_len));
                escape_buffer_len = 0;

                if (s.empty())
                    return;
            } else {
                for (size_t i = 0; i < escape_buffer_len; i++) {
                    text_console_print2(escape_buffer[i]);
                }

                escape_buffer_len = 0;
            }
        }
    }

loop:
    for (; !s.empty(); s = string_view(s.data() + 1, s.size() - 1)) {
        auto c = s.front();

        if (c == 0x1b) {
            if (s.size() == 1) {
                escape_buffer[0] = 0x1b;
                escape_buffer_len = 1;
                return;
            }

            if (s[1] == '[') {
                unsigned int param_bytes = 0;

                while (s.size() >= param_bytes + 2) {
                    if (s[2 + param_bytes] >= 0x30 && s[2 + param_bytes] <= 0x3f)
                        param_bytes++;
                    else
                        break;
                }

                if (s.size() == param_bytes + 2) {
                    if (param_bytes + 2 > sizeof(escape_buffer)) {
                        for (size_t i = 0; i < escape_buffer_len; i++) {
                            text_console_print2(escape_buffer[i]);
                        }
                        escape_buffer_len = 0;

                        for (size_t i = 0; i < s.size(); i++) {
                            text_console_print2(s[i]);
                        }

                        return;
                    }

                    memcpy(escape_buffer, s.data(), param_bytes + 2);
                    escape_buffer_len = param_bytes + 2;
                    return;
                }

                if (s[param_bytes + 2] >= 0x40 && s[param_bytes + 2] <= 0x7e) {
                    // cap mid-buffer escape sequences as sizeof(escape_buffer), for consistency
                    if (param_bytes + 3 > sizeof(escape_buffer)) {
                        for (size_t i = 0; i < param_bytes + 3; i++) {
                            text_console_print2(s[i]);
                        }
                    } else
                        handle_escape_sequence<false>(string_view(s.data(), param_bytes + 3));

                    s = string_view(s.data() + param_bytes + 2, s.size() - param_bytes - 2);
                    continue;
                }
            }
        }

        text_console_print2(c);
    }

    text_console_move_cursor((uint8_t)cursor_x, (uint8_t)cursor_y);
}

static void text_console_print(integral auto i) {
    char s[21];

    auto [ptr, ec] = to_chars(s, s + sizeof(s), i);
    text_console_print(string_view(s, ptr - s));
}

static void draw_ft_char(const console_info& c, unsigned int fb_x, unsigned int fb_y) {
    auto font = c.bold ? face_bold : face;

    auto error = FT_Load_Glyph(font, c.glyph, FT_LOAD_RENDER | FT_RENDER_MODE_MONO);
    if (error)
        return;

    auto& bitmap = font->glyph->bitmap;
    auto buf = bitmap.buffer;

    auto width = bitmap.width;
    if (fb_x + font->glyph->bitmap_left + width > fb_width)
        width = fb_width - fb_x - font->glyph->bitmap_left;

    int y = fb_y - font->glyph->bitmap_top;
    int bottom = y + bitmap.rows;

    if (y < 0) {
        buf += bitmap.width * (font->glyph->bitmap_top - fb_y);
        y = 0;
    }

    if (y >= (int)fb_height)
        return;

    if (bottom > (int)fb_height)
        bottom = fb_height;

    uint32_t bg_colour_r = c.bg >> 16;
    uint32_t fg_colour_r = c.fg >> 16;
    uint32_t bg_colour_g = (c.bg & 0xff00) >> 8;
    uint32_t fg_colour_g = (c.fg & 0xff00) >> 8;
    uint32_t bg_colour_b = c.bg & 0xff;
    uint32_t fg_colour_b = c.fg & 0xff;

    auto base = shadow_framebuffer.data() + (y * fb_pixels_per_scan_line) + fb_x + font->glyph->bitmap_left;

    while (y < bottom) {
        for (unsigned int x = 0; x < width; x++) {
            if (*buf == 255)
                base[x] = c.fg;
            else if (*buf != 0) {
                auto r = (uint8_t)(((bg_colour_r * (255 - (uint32_t)*buf)) + (fg_colour_r * (uint32_t)*buf)) / 255);
                auto g = (uint8_t)(((bg_colour_g * (255 - (uint32_t)*buf)) + (fg_colour_g * (uint32_t)*buf)) / 255);
                auto b = (uint8_t)(((bg_colour_b * (255 - (uint32_t)*buf)) + (fg_colour_b * (uint32_t)*buf)) / 255);

                base[x] = (r << 16) | (g << 8) | b;
            }

            buf++;
        }

        buf += bitmap.width - width;

        base += fb_pixels_per_scan_line;
        y++;
    }
}

static void fb_console_overflow() {
    memcpy(console, &console[fb_console_cols], (fb_console_rows - 1) * fb_console_cols * sizeof(console_info));

    auto c = &console[(fb_console_rows - 1) * fb_console_cols];

    auto font = bold ? face_bold : face;
    auto glyph = FT_Get_Char_Index(font, ' ');

    for (unsigned int i = 0; i < fb_console_cols; i++) {
        // FIXME - set fg and bg to current colour
        c->glyph = glyph;
        c++;
    }
}

static void rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, uint32_t col) {
    auto dest = shadow_framebuffer.data() + (fb_pixels_per_scan_line * y) + x;

    for (unsigned int i = 0; i < h; i++) {
        for (unsigned int j = 0; j < w; j++) {
            *dest = col;
            dest++;
        }

        dest += fb_pixels_per_scan_line - w;
    }
}

static void refresh_shadow_framebuffer() {
    auto* c = &console[0];

    for (unsigned int y = 0; y < fb_console_rows; y++) {
        for (unsigned int x = 0; x < fb_console_cols; x++) {
            rect(x * font_width, y * font_height, font_width, font_height, c->bg);
            draw_ft_char(*c, x * font_width, ((y + 1) * font_height) - font_descender);

            c++;
        }
    }
}

static void print_char(char32_t c) {
    if (c == '\n') {
        if (console) {
            auto font = bold ? face_bold : face;
            auto glyph = FT_Get_Char_Index(font, ' ');

            for (unsigned int i = cursor_x; i < fb_console_cols; i++) {
                console[(cursor_y * fb_console_cols) + i].glyph = glyph;
            }
        }

        cursor_x = 0;
        cursor_y++;

        if (cursor_y == fb_console_rows) {
            fb_console_overflow();
            cursor_y = fb_console_rows - 1;
        }
    } else if (c == '\b') {
        if (cursor_x == 0 && cursor_y == 0)
            return;

        if (cursor_x > 0)
            cursor_x--;
        else {
            cursor_x = fb_console_cols - 1;
            cursor_y--;

            if (cursor_x != fb_console_cols - 1)
                cursor_x++;
        }

        if (console && console_output_mode & ENABLE_PROCESSED_OUTPUT) {
            auto font = bold ? face_bold : face;
            auto glyph = FT_Get_Char_Index(font, ' ');

            console[(cursor_y * fb_console_cols) + cursor_x].glyph = glyph;
        }
    } else {
        if (console) {
            auto& p = console[(cursor_y * fb_console_cols) + cursor_x];

            auto font = bold ? face_bold : face;
            auto glyph = FT_Get_Char_Index(font, c);

            p.glyph = glyph;
            p.bg = bg_colour;
            p.fg = fg_colour;
            p.bold = bold;
        }

        cursor_x++;

        if (cursor_x == fb_console_cols) {
            cursor_x = 0;
            cursor_y++;

            if (cursor_y == fb_console_rows) {
                fb_console_overflow();
                cursor_y = fb_console_rows - 1;
            }
        }
    }
}

static void refresh_framebuffer() {
    // FIXME - only copy changed parts?

    memcpy_aligned(framebuffer.data(), shadow_framebuffer.data(), framebuffer.size_bytes());
}

static void fb_print2(char c) {
    if (utf8_chars != 0) {
        if (!(c & 0x80) || utf8_chars == sizeof(utf8_buf)) {
            print_char(0xfffd);
            print_char(c);
            utf8_chars = 0;
            return;
        }

        utf8_buf[utf8_chars] = c;
        utf8_chars++;

        if ((utf8_buf[0] & 0xe0) == 0xc0 && utf8_chars == 2) {
            if ((utf8_buf[1] & 0xc0) != 0x80) {
                print_char(0xfffd);
                utf8_chars = 0;
                return;
            }

            char32_t c = ((utf8_buf[0] & 0x1f) << 6) | (utf8_buf[1] & 0x3f);

            print_char(c);
            utf8_chars = 0;
            return;
        } else if ((utf8_buf[0] & 0xf0) == 0xe0) {
            if (utf8_chars == 2) {
                if ((utf8_buf[1] & 0xc0) != 0x80) {
                    print_char(0xfffd);
                    utf8_chars = 0;
                }
            } else {
                if ((utf8_buf[2] & 0xc0) != 0x80) {
                    print_char(0xfffd);
                    utf8_chars = 0;
                    return;
                }

                char32_t c = ((utf8_buf[0] & 0xf) << 12) | ((utf8_buf[1] & 0x3f) << 6) | (utf8_buf[2] & 0x3f);

                print_char(c);
                utf8_chars = 0;
            }

            return;
        } else if ((utf8_buf[0] & 0xf8) == 0xf0) {
            if (utf8_chars == 2) {
                if ((utf8_buf[1] & 0xc0) != 0x80) {
                    print_char(0xfffd);
                    utf8_chars = 0;
                }
            } else if (utf8_chars == 3) {
                if ((utf8_buf[2] & 0xc0) != 0x80) {
                    print_char(0xfffd);
                    utf8_chars = 0;
                }
            } else {
                if ((utf8_buf[3] & 0xc0) != 0x80) {
                    print_char(0xfffd);
                    utf8_chars = 0;
                    return;
                }

                char32_t c = ((utf8_buf[0] & 0x7) << 18) | ((utf8_buf[1] & 0x3f) << 12) | ((utf8_buf[2] & 0x3f) << 6) | (utf8_buf[3] & 0x3f);

                print_char(c);
                utf8_chars = 0;
            }

            return;
        }
    }

    if ((c & 0xc0) == 0xc0) { // first character of UTF-8 sequence
        utf8_buf[0] = c;
        utf8_chars = 1;
        return;
    } else if (c & 0x80) {
        print_char(0xfffd);
        return;
    }

    print_char(c);
}

static void fb_print(string_view s) {
    if (escape_buffer_len != 0) {
        if (escape_buffer_len == 1 && s[0] != '[') {
            fb_print2(0x1b);
            escape_buffer_len = 0;
        } else {
            if (escape_buffer_len == 1) {
                escape_buffer[1] = '[';
                escape_buffer_len++;

                s = string_view(s.data() + 1, s.size() - 1);

                if (s.empty())
                    return;
            }

            while (!s.empty() && s.front() >= 0x30 && s.front() <= 0x3f) {
                if (escape_buffer_len == sizeof(escape_buffer)) {
                    for (size_t i = 0; i < escape_buffer_len; i++) {
                        fb_print2(escape_buffer[i]);
                    }

                    escape_buffer_len = 0;
                    goto loop;
                }

                escape_buffer[escape_buffer_len] = s.front();
                escape_buffer_len++;
                s = string_view(s.data() + 1, s.size() - 1);
            }

            if (s.empty())
                return;

            if (s.front() >= 0x40 && s.front() <= 0x7e) {
                escape_buffer[escape_buffer_len] = s.front();
                escape_buffer_len++;

                s = string_view(s.data() + 1, s.size() - 1);

                handle_escape_sequence<true>(string_view(escape_buffer, escape_buffer_len));
                escape_buffer_len = 0;

                if (s.empty())
                    return;
            } else {
                for (size_t i = 0; i < escape_buffer_len; i++) {
                    fb_print2(escape_buffer[i]);
                }

                escape_buffer_len = 0;
            }
        }
    }

loop:
    for (; !s.empty(); s = string_view(s.data() + 1, s.size() - 1)) {
        auto c = s.front();

        if (c == 0x1b) {
            if (s.size() == 1) {
                escape_buffer[0] = 0x1b;
                escape_buffer_len = 1;
                return;
            }

            if (s[1] == '[') {
                unsigned int param_bytes = 0;

                while (s.size() >= param_bytes + 2) {
                    if (s[2 + param_bytes] >= 0x30 && s[2 + param_bytes] <= 0x3f)
                        param_bytes++;
                    else
                        break;
                }

                if (s.size() == param_bytes + 2) {
                    if (param_bytes + 2 > sizeof(escape_buffer)) {
                        for (size_t i = 0; i < escape_buffer_len; i++) {
                            fb_print2(escape_buffer[i]);
                        }
                        escape_buffer_len = 0;

                        for (size_t i = 0; i < s.size(); i++) {
                            fb_print2(s[i]);
                        }

                        return;
                    }

                    memcpy(escape_buffer, s.data(), param_bytes + 2);
                    escape_buffer_len = param_bytes + 2;
                    return;
                }

                if (s[param_bytes + 2] >= 0x40 && s[param_bytes + 2] <= 0x7e) {
                    // cap mid-buffer escape sequences as sizeof(escape_buffer), for consistency
                    if (param_bytes + 3 > sizeof(escape_buffer)) {
                        for (size_t i = 0; i < param_bytes + 3; i++) {
                            fb_print2(s[i]);
                        }
                    } else
                        handle_escape_sequence<true>(string_view(s.data(), param_bytes + 3));

                    s = string_view(s.data() + param_bytes + 2, s.size() - param_bytes - 2);
                    continue;
                }
            }
        }

        fb_print2(c);
    }
}

static void fb_print(integral auto i) {
    char s[21];

    auto [ptr, ec] = to_chars(s, s + sizeof(s), i);
    fb_print(string_view(s, ptr - s));
}

static void early_print(string_view s) {
    if (framebuffer.empty()) {
        text_console_print(s);
        return;
    }

    if (!fb_console_started)
        return;

    fb_print(s);
    refresh_shadow_framebuffer();
    refresh_framebuffer();
}

void safe_print(string_view s) {
    write(console_pipe, s);

    if (com1_pipe_handle)
        write(com1_pipe_handle, s);
}

void print_hex(uint64_t num, unsigned int padding) {
    char s[17];
    span sp(s, sizeof(s));

    sp = sp_itoa_hex(sp, num, padding);

    printk(string_view(s, sp.data() - s));
}

bool init_console(const loader_block_framebuffer& fb,
                  const loader_block_text_console& tc) {
    if (!has_framebuffer) {
        console_cols = tc.cols;
        console_rows = tc.rows;

        text_console = span((uint16_t*)phys_address(tc.phys_address),
                            console_cols * console_rows);

        // clear console (foreground colour needs to be initialized for cursor to show up)
        for (uint8_t y = 0; y < console_rows; y++) {
            for (uint8_t x = 0; x < console_cols; x++) {
                text_console_putc(x, y, ' ', console_colour::black, console_colour::light_grey);
            }
        }
    } else {
        uint32_t size = fb.pixels_per_scan_line * fb.height * sizeof(uint32_t);

        if (size == 0) {
            printk("Framebuffer size was 0.\n");
            return false;
        }

        auto pages = bytes_to_pages(size);
        auto va = allocate_va(pages);
        if (!va) {
            printk("allocate_va failed\n");
            return false;
        }

        framebuffer = span((uint32_t*)va, fb.pixels_per_scan_line * fb.height);

        for (unsigned int i = 0; i < pages; i++) {
            if (!map_page((uintptr_t)fb.phys_address + (i << PAGE_SHIFT), va + (i << PAGE_SHIFT), mapping_type::write_combine)) {
                printk("map_page failed\n");
                return false;
            }
        }

        va = allocate_va(pages);
        if (!va) {
            printk("allocate_va failed\n");
            return false;
        }

        shadow_framebuffer = span((uint32_t*)va, fb.pixels_per_scan_line * fb.height);

        for (unsigned int i = 0; i < pages; i++) {
            auto pa = allocate_page();
            if (!pa) {
                printk("allocate_page failed\n");
                return false;
            }

            if (!map_page(pa, va + (i << PAGE_SHIFT), mapping_type::read_write)) {
                printk("map_page failed\n");
                return false;
            }
        }

        fb_width = fb.width;
        fb_height = fb.height;
        fb_pixels_per_scan_line = fb.pixels_per_scan_line;

        memset(framebuffer.data(), 0, framebuffer.size_bytes());
        memset(shadow_framebuffer.data(), 0, shadow_framebuffer.size_bytes());
    }

    return true;
}

static void console_event_recv() {
    auto buf_size = console_buffer_pages << PAGE_SHIFT;

    auto buf = (uint8_t*)malloc(buf_size);
    if (!buf)
        return;

    do {
        uint64_t read_bytes;

        wait_for_object(console_pipe.get(), false);

        auto status = static_cast<pipe&>(console_pipe.get()).read(span(buf, buf_size), read_bytes);

        // FIXME - watch out for partial UTF-8 characters, partial escape sequences, etc.

        if (success(status) && read_bytes != 0)
            early_print(string_view((char*)buf, read_bytes));
    } while (true);

    free(buf);
}

bool init_console_pipe() {
    console_pipe = create_pipe(console_buffer_pages);
    if (!console_pipe) {
        printk("create_pipe failed\n");
        return false;
    }

    if (!name_object(console_pipe.get(), "tty")) {
        printk("name_object failed\n");
        return false;
    }

    if (framebuffer.empty()) { // gets started later if we have framebuffer
        if (!add_kernel_thread(console_event_recv)) {
            printk("add_kernel_thread failed\n");
            return false;
        }
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

bool init_fb_console() {
    FT_Error error;
    unsigned int dpi;

    error = FT_Init_FreeType(&ft);
    if (error) {
        printk("FT_Init_FreeType failed (");
        print_hex(error);
        printk(")\n");

        return false;
    }

    if (have_edid) {
        uint8_t screen_x_cm = edid[21];

        dpi = (fb_width * 254) / ((uint16_t)screen_x_cm * 100);
    } else
        dpi = 72;

    error = FT_New_Memory_Face(ft, (uint8_t*)&console_font_data, (FT_Long)console_font_size,
                               0, &face);
    if (error) {
        printk("FT_New_Memory_Face failed (");
        print_hex(error);
        printk(")\n");

        return false;
    }

    error = FT_Set_Char_Size(face, 18 * 64, 0, dpi, 0);
    if (error) {
        printk("FT_Set_Char_Size failed (");
        print_hex(error);
        printk(")\n");

        return false;
    }

    error = FT_New_Memory_Face(ft, (uint8_t*)&console_font_bold_data, (FT_Long)console_font_bold_size,
                               0, &face_bold);
    if (error) {
        printk("FT_New_Memory_Face failed (");
        print_hex(error);
        printk(")\n");

        return false;
    }

    error = FT_Set_Char_Size(face_bold, 18 * 64, 0, dpi, 0);
    if (error) {
        printk("FT_Set_Char_Size failed (");
        print_hex(error);
        printk(")\n");

        return false;
    }

    error = FT_Load_Char(face, 'M', FT_LOAD_RENDER | FT_RENDER_MODE_MONO);
    if (error) {
        printk("FT_Set_Char_Size failed (");
        print_hex(error);
        printk(")\n");

        return false;
    }

    font_width = face->glyph->bitmap.width;
    font_height = face->size->metrics.height / 64;
    font_descender = -face->size->metrics.descender / 64;

    fb_console_cols = fb_width / font_width;
    fb_console_rows = fb_height / font_height;

    cursor_x = cursor_y = 0;

    console = (console_info*)malloc(fb_console_cols * fb_console_rows * sizeof(console_info));
    if (!console) {
        printk("out of memory\n");
        return false;
    }

    auto glyph = FT_Get_Char_Index(face, ' ');

    for (unsigned int i = 0; i < fb_console_cols * fb_console_rows; i++) {
        console[i].bg = DEFAULT_BG_COLOUR;
        console[i].fg = DEFAULT_FG_COLOUR;
        console[i].bold = false;
        console[i].glyph = glyph;
    }

    if (!add_kernel_thread(console_event_recv)) {
        printk("add_kernel_thread failed\n");
        return false;
    }

    fb_console_started = true;

    return true;
}

static void fill_text_console(enum console_colour bg, enum console_colour fg) {
    for (auto& c : text_console) {
        c = (uint16_t)(((uint16_t)bg << 12) | ((uint16_t)fg << 8) | ' ');
    }
}

static void fill_framebuffer(uint32_t colour) {
    auto buf = shadow_framebuffer.data();

    for (unsigned int y = 0; y < fb_height; y++) {
        for (unsigned int x = 0; x < fb_width; x++) {
            *buf = colour;
            buf++;
        }

        buf += fb_pixels_per_scan_line - fb_width;
    }

    memcpy(framebuffer.data(), shadow_framebuffer.data(), framebuffer.size_bytes());
}

static void disable_cursor() {
    outb(0x3d4, 0xa);
    outb(0x3d5, 0x20);
}

static kernel_image* get_kernel_image_from_address(uint64_t addr) {
    auto le = kernel_images.next;

    while (le != &kernel_images) {
        auto& ent = containing_record<kernel_image>(le);

        if (addr >= (uintptr_t)ent.base_address && addr < (uintptr_t)ent.base_address + ent.size)
            return &ent;

        le = le->next;
    }

    return nullptr;
}

template<bool FB>
static void print_rip(uint64_t rip) {
    char s[200];
    auto sp = span(s, sizeof(s));

    sp = sp_itoa_hex(sp, rip, 16);

    auto img = get_kernel_image_from_address(rip);

    if (img) {
        // FIXME - also get filename and line no. from DWARF

        auto [func_name, func_start] = dwarf_find_function(*img, rip);

        sp = sp_strcpy(sp, " (");
        sp = sp_strcpy(sp, img->name);

        if (!func_name.empty()) {
            sp = sp_strcpy(sp, "!");
            sp = sp_strcpy(sp, func_name);

            if (rip != func_start) {
                sp = sp_strcpy(sp, "+");
                sp = sp_itoa_hex(sp, rip - func_start);
            }
        } else {
            sp = sp_strcpy(sp, "+");
            sp = sp_itoa_hex(sp, rip - (uintptr_t)img->base_address);
        }

        sp = sp_strcpy(sp, ")");
    }

    sp = sp_strcpy(sp, "\n");

    if constexpr (FB)
        fb_print(string_view(s, sp.data() - s));
    else
        text_console_print(string_view(s, sp.data() - s));
}

static const RUNTIME_FUNCTION* find_runtime_function(const kernel_image& img,
                                                     uint64_t rip) {
    rip -= (uintptr_t)img.base_address;

    auto pdata = find_pdata(img);
    if (pdata.empty())
        return nullptr;

    span<RUNTIME_FUNCTION> rt_funcs((RUNTIME_FUNCTION*)pdata.data(),
                                    pdata.size() / sizeof(RUNTIME_FUNCTION));

    for (const auto& f : rt_funcs) {
        if (rip >= f.BeginAddress && rip < f.EndAddress)
            return &f;

        if (f.BeginAddress > rip) // stored in order
            break;
    }

    return nullptr;
}

static optional<uint64_t> read_uint64_t(uint64_t addr) {
    // FIXME - do this safely
    return *(uint64_t*)addr;
}

enum class unwind_opcode {
    UWOP_PUSH_NONVOL = 0,
    UWOP_ALLOC_LARGE = 1,
    UWOP_ALLOC_SMALL = 2,
    UWOP_SET_FPREG = 3,
    UWOP_SAVE_NONVOL = 4,
    UWOP_SAVE_NONVOL_FAR = 5,
    UWOP_SAVE_XMM128 = 8,
    UWOP_SAVE_XMM128_FAR = 9,
    UWOP_PUSH_MACHFRAME = 10,
};

enum class unwind_register {
    rax = 0,
    rcx = 1,
    rdx = 2,
    rbx = 3,
    rsp = 4,
    rbp = 5,
    rsi = 6,
    rdi = 7,
    r8 = 8,
    r9 = 9,
    r10 = 10,
    r11 = 11,
    r12 = 12,
    r13 = 13,
    r14 = 14,
    r15 = 15
};

static uint64_t get_register(const CONTEXT& context, enum unwind_register reg) {
    switch (reg) {
        case unwind_register::rax:
            return context.Rax;
        case unwind_register::rcx:
            return context.Rcx;
        case unwind_register::rdx:
            return context.Rdx;
        case unwind_register::rbx:
            return context.Rbx;
        case unwind_register::rsp:
            return context.Rsp;
        case unwind_register::rbp:
            return context.Rbp;
        case unwind_register::rsi:
            return context.Rsi;
        case unwind_register::rdi:
            return context.Rdi;
        case unwind_register::r8:
            return context.R8;
        case unwind_register::r9:
            return context.R9;
        case unwind_register::r10:
            return context.R10;
        case unwind_register::r11:
            return context.R11;
        case unwind_register::r12:
            return context.R12;
        case unwind_register::r13:
            return context.R13;
        case unwind_register::r14:
            return context.R14;
        case unwind_register::r15:
            return context.R15;
    }

    return 0;
}

static void set_register(CONTEXT& context, enum unwind_register reg, uint64_t value) {
    switch (reg) {
        case unwind_register::rax:
            context.Rax = value;
            break;
        case unwind_register::rcx:
            context.Rcx = value;
            break;
        case unwind_register::rdx:
            context.Rdx = value;
            break;
        case unwind_register::rbx:
            context.Rbx = value;
            break;
        case unwind_register::rsp:
            context.Rsp = value;
            break;
        case unwind_register::rbp:
            context.Rbp = value;
            break;
        case unwind_register::rsi:
            context.Rsi = value;
            break;
        case unwind_register::rdi:
            context.Rdi = value;
            break;
        case unwind_register::r8:
            context.R8 = value;
            break;
        case unwind_register::r9:
            context.R9 = value;
            break;
        case unwind_register::r10:
            context.R10 = value;
            break;
        case unwind_register::r11:
            context.R11 = value;
            break;
        case unwind_register::r12:
            context.R12 = value;
            break;
        case unwind_register::r13:
            context.R13 = value;
            break;
        case unwind_register::r14:
            context.R14 = value;
            break;
        case unwind_register::r15:
            context.R15 = value;
            break;
    }
}

static bool unwind_context2(CONTEXT& context, const kernel_image& img,
                            const RUNTIME_FUNCTION* prt_func,
                            bool& machine_frame) {
    uint32_t func_offset;
    uint64_t frame = context.Rsp;

    while (true) {
        const auto& rt_func = *prt_func;

        if (rt_func.UnwindInfoAddress > img.size - sizeof(UNWIND_INFO))
            return false;

        auto& ui = *(UNWIND_INFO*)((uint8_t*)img.base_address + rt_func.UnwindInfoAddress);

        if (ui.version != 1)
            return false;

        if (ui.unwind_codes_count == 0)
            return true;

        if (ui.unwind_codes_count > 0) {
            if (rt_func.UnwindInfoAddress > img.size - sizeof(UNWIND_INFO) - (ui.unwind_codes_count * sizeof(UNWIND_CODE)))
                return false;

            if (context.Rip >= (uintptr_t)img.base_address + rt_func.BeginAddress &&
                context.Rip < (uintptr_t)img.base_address + rt_func.BeginAddress + ui.prologue_bytes) {
                func_offset = (uint32_t)(context.Rip - (uintptr_t)img.base_address - rt_func.BeginAddress);
            } else
                func_offset = 0xffffffff;

            span codes((UNWIND_CODE*)((uint8_t*)img.base_address + rt_func.UnwindInfoAddress + sizeof(UNWIND_INFO)),
                       ui.unwind_codes_count);

            if (ui.frame_register != 0) {
                frame = get_register(context, (enum unwind_register)ui.frame_register);
                frame -= ui.frame_offset * 16;
            }

            for (size_t i = 0; i < codes.size(); i++) {
                const auto& c = codes[i];

                if (c.offset > func_offset)
                    continue;

                switch ((enum unwind_opcode)c.code) {
                    case unwind_opcode::UWOP_PUSH_NONVOL: { // push reg (.seh_pushreg)
                        auto val = read_uint64_t(context.Rsp);

                        if (!val.has_value())
                            return false;

                        set_register(context, (enum unwind_register)c.info, *val);
                        context.Rsp += sizeof(uint64_t);
                        break;
                    }

                    case unwind_opcode::UWOP_ALLOC_LARGE: // sub rsp, n (.seh_stackalloc)
                        if (c.info == 0) {
                            if (i == codes.size() - 1)
                                return false;

                            context.Rsp += *(uint16_t*)&codes[i + 1] * 8;
                            i++;
                        } else {
                            if (i >= codes.size() - 2)
                                return false;

                            context.Rsp += *(uint32_t*)&codes[i + 1];
                            i += 2;
                        }
                        break;

                    case unwind_opcode::UWOP_ALLOC_SMALL: // sub rsp, n (.seh_stackalloc)
                        context.Rsp += (c.info * 8) + 8;
                        break;

                    case unwind_opcode::UWOP_SET_FPREG: // lea framereg, [rsp + n] (.seh_setframe)
                        context.Rsp = frame;
                        break;

                    case unwind_opcode::UWOP_SAVE_NONVOL: // mov [rsp+n], reg (.seh_savereg)
                        if (i == codes.size() - 1)
                            return false;

                        set_register(context, (enum unwind_register)c.info,
                                    frame + *(uint16_t*)&codes[i + 1] * 8);
                        i++;
                        break;

                    case unwind_opcode::UWOP_SAVE_NONVOL_FAR: // mov [rsp+n], reg (.seh_savereg)
                        if (i >= codes.size() - 2)
                            return false;

                        set_register(context, (enum unwind_register)c.info,
                                    frame + *(uint32_t*)&codes[i + 1]);
                        i += 2;
                        break;

                    case unwind_opcode::UWOP_SAVE_XMM128: // movaps [rsp+n], xmmreg (.seh_savexmm)
                        if (i == codes.size() - 1)
                            return false;

                        i++;
                        break;

                    case unwind_opcode::UWOP_SAVE_XMM128_FAR: // movaps [rsp+n], xmmreg (.seh_savexmm)
                        if (i >= codes.size() - 2)
                            return false;

                        i += 2;
                        break;

                    case unwind_opcode::UWOP_PUSH_MACHFRAME: { // interrupt (.seh_pushframe)
                        if (c.info != 0)
                            context.Rsp += sizeof(uint64_t);

                        auto val = read_uint64_t(context.Rsp);

                        if (!val.has_value())
                            return false;

                        context.Rip = *val;

                        val = read_uint64_t(context.Rsp + (3 * sizeof(uint64_t)));

                        if (!val.has_value())
                            return false;

                        context.Rsp = *val;
                        machine_frame = true;
                        break;
                    }

                    default:
                        return false;
                }
            }
        }

        if (!(ui.flags & UNW_FLAG_CHAININFO))
            break;

        uint8_t* addr = (uint8_t*)img.base_address;
        addr += rt_func.UnwindInfoAddress;
        addr += ui.unwind_codes_count * sizeof(UNWIND_CODE);

        if (ui.unwind_codes_count & 1)
            addr += sizeof(UNWIND_CODE);

        prt_func = (RUNTIME_FUNCTION*)addr;
    }

    return true;
}

static bool unwind_context(CONTEXT& context) {
    bool machine_frame = false;

    auto img = get_kernel_image_from_address(context.Rip);
    if (!img)
        return false;

    auto rt_func = find_runtime_function(*img, context.Rip);

    // RUNTIME_FUNCTION isn't compulsory for leaf functions with no stack
    if (rt_func) {
        if (!unwind_context2(context, *img, rt_func, machine_frame))
            return false;
    }

    if (!machine_frame) {
        auto rip = read_uint64_t(context.Rsp);

        if (!rip.has_value())
            return false;

        context.Rsp += sizeof(uint64_t);
        context.Rip = *rip;
    }

    return true;
}

static void print_stacktrace(CONTEXT& context, const invocable<uint64_t> auto& func) {
    // not interested in top entry (bsod function)
    if (!unwind_context(context))
        return;

    do {
        func(context.Rip);

        if (!unwind_context(context))
            return;
    } while (true);
}

void bsod(string_view msg, source_location sl) {
    __asm("cli");

    lock_guard lock(bsod_lock);

    // send IPI to other cores to stop them

    auto icr1 = (volatile uint32_t*)&local_apic[0x300];

    *icr1 = 0xc0000 | halt_interrupt;

    cursor_x = 0;
    cursor_y = 0;

    // clear screen to blue and print message

    if (framebuffer.empty()) {
        disable_cursor();
        fill_text_console(console_colour::blue, console_colour::white);

        text_console_print("\e[97;44m");

        for (unsigned int i = 0; i < (console_cols / 2) - 2; i++) {
            fb_print(" ");
        }

        text_console_print("\e[1mBSOD\e[22m\n\n");

        text_console_print(msg);
        text_console_print("\n");
        text_console_print(sl.file_name());
        text_console_print(":");
        text_console_print(sl.line());
        text_console_print(", ");
        text_console_print(sl.function_name());
        text_console_print("\n");
    } else {
        fill_framebuffer(BSOD_BG_COLOUR);

        auto glyph = FT_Get_Char_Index(face, ' ');

        auto c = console;
        for (unsigned int y = 0; y < fb_console_rows; y++) {
            for (unsigned int x = 0; x < fb_console_cols; x++) {
                c->bg = BSOD_BG_COLOUR;
                c->fg = BSOD_FG_COLOUR;
                c->glyph = glyph;
                c++;
            }
        }

        fb_print("\e[97;44m");

        for (unsigned int i = 0; i < (fb_console_cols / 2) - 2; i++) {
            fb_print(" ");
        }

        fb_print("\e[1mBSOD\e[22m\n\n");

        fb_print(msg);
        fb_print("\n");
        fb_print(sl.file_name());
        fb_print(":");
        fb_print(sl.line());
        fb_print(", ");
        fb_print(sl.function_name());
        fb_print("\n");
    }

    // FIXME - print context?

    CONTEXT context;

    __asm __volatile(
        "mov %0, rsp\n"
        "\tmov %1, rbp\n"
        "\tcall 0f\n"
        "\t0:\n"
        "\tpop rax\n"
        "\tmov %2, rax\n"
        : "=m" (context.Rsp), "=m" (context.Rbp), "=m" (context.Rip)
        :
        : "rax"
    );

    if (framebuffer.empty()) {
        text_console_print("\n");
        print_stacktrace(context, print_rip<false>);
    } else {
        fb_print("\n");
        print_stacktrace(context, print_rip<true>);

        refresh_shadow_framebuffer();
        refresh_framebuffer();
    }

    __asm("call halt_isr");
    __builtin_unreachable();
}

void _assert(const char* msg, const char* file, unsigned int line) {
    span sp(assertion_buffer, sizeof(assertion_buffer));

    sp = sp_strcpy(sp, "assertion failed at ");
    sp = sp_strcpy(sp, file);
    sp = sp_strcpy(sp, ":");
    sp = sp_itoa(sp, line);
    sp = sp_strcpy(sp, ": ");
    sp = sp_strcpy(sp, msg);

    bsod(string_view(assertion_buffer, sp.data() - assertion_buffer));
}

void copy_edid(loader_block* lb) {
    memcpy(edid, (void*)lb->edid, EDID_SIZE);
    have_edid = true;
}
