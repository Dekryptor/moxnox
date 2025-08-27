#include "nt.h"
#include <ntdll.h>
#include <pe.h>
#include <string_view>
#include <tuple>
#include <span>
#include <memory>
#include <array>
#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftbitmap.h>
#include <hb.h>
#include <hb-ft.h>

import format;
import rbtree;
import unique_handle;
import list;
import vt100;
import print;
import signal;
import unicode;
import cp437;
import text;
import cmdline;
import evdev;
import keyboard;

using namespace std;

static const size_t PAGE_SIZE = 4096;
static const uint64_t PAGE_SHIFT = 12;

static const string_view welcome_message = "\nWelcome to \e[1mMoxnox\e[m\n\n";

extern uint64_t __CTOR_LIST__;

struct glyph_info {
    // FIXME - destructor

    rbtree_node<FT_UInt> tree;
    FT_Int bitmap_left;
    FT_Int bitmap_top;
    FT_Bitmap bitmap;
};

struct font_info {
    // FIXME - destructor

    FT_Face face;
    hb_font_t* hb_font;
    rbtree_node<FT_UInt>* glyphs = nullptr; // FIXME - destructor for this
};

struct console_cell {
    hb_glyph_position_t pos;
    FT_UInt glyph;
    uint32_t fg_colour;
    uint32_t bg_colour;
    unsigned int bold : 1;
    unsigned int italic : 1;
};

class hb_buffer_closer {
public:
    using pointer = hb_buffer_t*;

    void operator()(hb_buffer_t* buf) {
        hb_buffer_destroy(buf);
    }
};

using unique_hb_buffer = unique_ptr<hb_buffer_t*, hb_buffer_closer>;

struct console_line {
    ~console_line() {
        if (text)
            free(text);
    }

    list_entry le;
    char32_t* text = nullptr;
    size_t len = 0;
    unsigned int num_cells = 0;
    bool changed = false;
    console_cell cells[0];
};

static FT_Library ft;
static font_info font, bold_font, italic_font, bold_italic_font;
static MN_SECTION_FRAMEBUFFER_INFORMATION fb_info;
static span<uint32_t> framebuffer;
static unsigned int font_width, font_height, font_descender;

static console_line* curline = nullptr;
static list_entry lines = { &lines, &lines };
static unsigned int num_lines = 0;

struct console_fb {
    void console_overflow(unsigned int rows = 1);
    static void handle_sgr_part_num(unsigned int num);
    void print_char(char32_t c, bool no_move);
    void cursor_changed();
    void scroll_down(unsigned int rows);
    void enable_cursor(bool enable);
    void move_cursor() { }

    unsigned int cursor_x, cursor_y;
    unsigned int console_rows, console_cols;
    bool show_cursor = true;
};

static console_fb console_fb_inst;

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

static const unsigned int DEFAULT_BG_COLOUR_INDEX = 0;
static const unsigned int DEFAULT_FG_COLOUR_INDEX = 7;
static const uint32_t DEFAULT_BG_COLOUR = vt100_colours[DEFAULT_BG_COLOUR_INDEX];
static const uint32_t DEFAULT_FG_COLOUR = vt100_colours[DEFAULT_FG_COLOUR_INDEX];
static uint32_t bg_colour_index = 0, fg_colour_index = 7;
static bool bg_bright = false, fg_bright = false, bold = false, italic = false;
static uint32_t bg_colour = DEFAULT_BG_COLOUR, fg_colour = DEFAULT_FG_COLOUR;

static constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

void* operator new(size_t count) {
    return malloc(count);
}

void operator delete(void* ptr) noexcept {
    free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    free(ptr);
}

void do_print(string_view s) {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    NtWriteFile(peb->ProcessParameters->StandardError, 0, nullptr, nullptr, &iosb, s.data(),
                (uint32_t)s.size(), nullptr, nullptr);
}

void console_fb::console_overflow(unsigned int rows) {
    auto line_size = offsetof(console_line, cells) * (sizeof(console_cell) * console_fb_inst.console_cols);

    for (unsigned int i = 0; i < rows; i++) {
        auto nextline = (console_line*)malloc(line_size);
        if (!nextline) {
            print("malloc failed\n");
            return;
        }

        new (nextline) console_line;

        for (unsigned int i = 0; i < console_fb_inst.console_cols; i++) {
            nextline->cells[i].fg_colour = fg_colour;
            nextline->cells[i].bg_colour = bg_colour;
        }

        insert_tail_list(lines, nextline->le);
        nextline->changed = true;
        num_lines++;
    }

    while (num_lines > console_fb_inst.console_rows) {
        auto& first = containing_record<console_line>(lines.next);

        remove_entry_list(first.le);
        delete &first;

        num_lines--;
    }

    for (auto le = lines.next; le != &lines; le = le->next) {
        auto& l = containing_record<console_line>(le);

        l.changed = true;
    }

    curline = &containing_record<console_line>(lines.prev);
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
            italic = false;
            break;

        case 1:
            bold = true;
            recalc_colours(true);
            break;

        case 3:
            italic = true;
            break;

        // FIXME - 4 - underline
        // FIXME - 5 - blink
        // FIXME - 7 - reverse video
        // FIXME - 9 - strikethrough
        // FIXME - 21 - double underline

        case 22:
            bold = false;
            recalc_colours(false);
            break;

        case 23:
            italic = false;
            break;

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

void console_fb::cursor_changed() {
    if (show_cursor)
        curline->changed = true;
}

void console_fb::scroll_down(unsigned int rows) {
    auto line_size = offsetof(console_line, cells) * (sizeof(console_cell) * console_fb_inst.console_cols);

    while (rows > 0) {
        auto firstline = (console_line*)malloc(line_size);
        if (!firstline) {
            print("malloc failed\n");
            return;
        }

        new (firstline) console_line;

        for (unsigned int i = 0; i < console_fb_inst.console_cols; i++) {
            firstline->cells[i].fg_colour = fg_colour;
            firstline->cells[i].bg_colour = bg_colour;
        }

        insert_head_list(lines, firstline->le);
        firstline->changed = true;
        num_lines++;
        cursor_y++;
        rows--;
    }

    while (num_lines > console_fb_inst.console_rows) {
        auto& first = containing_record<console_line>(lines.prev);

        remove_entry_list(first.le);
        delete &first;

        num_lines--;
    }

    if (cursor_y >= num_lines)
        cursor_y = num_lines - 1;

    for (auto le = lines.next; le != &lines; le = le->next) {
        auto& l = containing_record<console_line>(le);

        l.changed = true;
    }

    curline = &containing_record<console_line>(lines.prev);
}

void console_fb::enable_cursor(bool enable) {
    show_cursor = enable;
}

static bool init_font(span<const uint8_t> buf, font_info& font,
                      bool main_font) {
    FT_Error error;
    unsigned int font_size = 12; // FIXME - allow setting somehow

    error = FT_New_Memory_Face(ft, buf.data(), (FT_Long)buf.size(),
                               0, &font.face);
    if (error) {
        print("FT_New_Memory_Face failed ({:x})\n", error);
        return false;
    }

    error = FT_Set_Char_Size(font.face, (FT_F26Dot6)(font_size * 64), 0,
                             fb_info.dpi, 0);
    if (error) {
        print("FT_Set_Char_Size failed ({:x})\n", error);
        return false;
    }

    font.hb_font = hb_ft_font_create(font.face, nullptr);
    if (!font.hb_font) {
        print("hb_ft_font_create failed\n");
        FT_Done_Face(font.face);
        return false;
    }

    if (main_font) {
        font_width = (unsigned int)(font.face->size->metrics.max_advance / 64);
        font_height = (unsigned int)(font.face->size->metrics.height / 64);
        font_descender = (unsigned int)(-font.face->size->metrics.descender / 64);

        console_fb_inst.console_cols = fb_info.width / font_width;
        console_fb_inst.console_rows = fb_info.height / font_height;

        auto line_size = offsetof(console_line, cells) * (sizeof(console_cell) * console_fb_inst.console_cols);

        curline = (console_line*)malloc(line_size);
        if (!curline) {
            print("malloc failed\n");
            FT_Done_Face(font.face);
            FT_Done_FreeType(ft);
            return false;
        }

        new (curline) console_line;

        for (unsigned int i = 0; i < console_fb_inst.console_cols; i++) {
            curline->cells[i].fg_colour = fg_colour;
            curline->cells[i].bg_colour = bg_colour;
        }

        insert_tail_list(lines, curline->le);
        num_lines++;
    }

    return true;
}

static span<uint32_t> init_framebuffer(bool& not_found) {
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
    if (status == nt_status::OBJECT_NAME_NOT_FOUND) {
        not_found = true;
        return {};
    } else if (!success(status)) {
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

static void draw_ft_char(const console_cell& cell, unsigned int fb_x, unsigned int fb_y,
                         bool is_cursor) {
    font_info* curfont;

    if (cell.bold && cell.italic)
        curfont = &bold_italic_font;
    else if (cell.bold)
        curfont = &bold_font;
    else if (cell.italic)
        curfont = &italic_font;
    else
        curfont = &font;

    auto gi = load_glyph(*curfont, cell.glyph);
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

    auto bg = is_cursor ? cell.fg_colour : cell.bg_colour;
    auto fg = is_cursor ? cell.bg_colour : cell.fg_colour;

    uint32_t bg_colour_r = bg >> 16;
    uint32_t fg_colour_r = fg >> 16;
    uint32_t bg_colour_g = (bg & 0xff00) >> 8;
    uint32_t fg_colour_g = (fg & 0xff00) >> 8;
    uint32_t bg_colour_b = bg & 0xff;
    uint32_t fg_colour_b = fg & 0xff;

    auto base = framebuffer.data() + ((uint32_t)y * fb_info.pixels_per_scan_line) + fb_x + gi->bitmap_left;

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

        base += fb_info.pixels_per_scan_line;
        y++;
    }
}

static void rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h,
                 uint32_t col) {
    auto dest = framebuffer.data() + (fb_info.pixels_per_scan_line * y) + x;

    for (unsigned int i = 0; i < h; i++) {
        for (unsigned int j = 0; j < w; j++) {
            *dest = col;
            dest++;
        }

        dest += fb_info.pixels_per_scan_line - w;
    }
}

static void print_line(console_line& line, unsigned int sy) {
    for (unsigned int i = 0; i < line.num_cells; i++) {
        double x_offset = line.cells[i].pos.x_offset / 64.0;
        double y_offset = line.cells[i].pos.y_offset / 64.0;

        bool is_cursor = console_fb_inst.show_cursor &&
            i == console_fb_inst.cursor_x &&
            sy == console_fb_inst.cursor_y;

        auto bg = is_cursor ? line.cells[i].fg_colour : line.cells[i].bg_colour;

        rect(i * font_width, sy * font_height, font_width, font_height, bg);

        draw_ft_char(line.cells[i],
                     (i * font_width) + (unsigned int)x_offset,
                     ((sy + 1) * font_height) + (unsigned int)y_offset - font_descender,
                     is_cursor);
    }

    for (unsigned int i = line.num_cells; i < console_fb_inst.console_cols; i++) {
        bool is_cursor = console_fb_inst.show_cursor &&
            i == console_fb_inst.cursor_x &&
            sy == console_fb_inst.cursor_y;

        auto bg = is_cursor ? line.cells[i].fg_colour : line.cells[i].bg_colour;

        rect(i * font_width, sy * font_height, font_width, font_height, bg);
    }
}

static void flush_framebuffer() {
    void* flush_addr = framebuffer.data();
    size_t flush_size = framebuffer.size_bytes();
    IO_STATUS_BLOCK iosb;

    auto status = NtFlushVirtualMemory(NtCurrentProcess(), &flush_addr,
                                       &flush_size, &iosb);
    if (!success(status)) {
        print("NtFlushVirtualMemory returned {}\n", status);
        return;
    }
}

void console_fb::print_char(char32_t c, bool no_move) {
    if (c == '\n') {
        console_overflow();

        if (!no_move) {
            cursor_x = 0;
            cursor_y++;

            if (cursor_y == console_rows)
                cursor_y = console_rows - 1;
        }

        return;
    }

    if (c == '\b') {
        if (cursor_x > 0)
            cursor_x--;
        else {
            cursor_x = console_cols - 1;
            cursor_y--;

            if (cursor_x != console_cols - 1)
                cursor_x++;
        }

        // FIXME - not if ENABLE_PROCESSED_OUTPUT disabled
        c = ' ';
        no_move = true;
    }

    unsigned int text_pos;

    // FIXME - allow multiple characters at once?

    // get current position in string

    auto orig_cursor_x = cursor_x;

    if (curline->len == 0)
        text_pos = 0;
    else {
        unique_hb_buffer buf{hb_buffer_create()};

        hb_buffer_add_codepoints(buf.get(), (const hb_codepoint_t*)curline->text,
                                (int)curline->len, 0, -1);
        hb_buffer_guess_segment_properties(buf.get());

        hb_shape(font.hb_font, buf.get(), nullptr, 0);

        auto len = hb_buffer_get_length(buf.get());
        auto info = hb_buffer_get_glyph_infos(buf.get(), nullptr);

        if (len > cursor_x)
            text_pos = info[cursor_x].cluster;
        else
            text_pos = info[len - 1].cluster + 1; // FIXME?
    }

    // append to existing buffer

    if (text_pos < curline->len) {
        curline->text[text_pos] = c;

        if (!no_move)
            cursor_x = text_pos + 1;
    } else {
        auto t = (char32_t*)malloc((curline->len + 1) * sizeof(char32_t));
        if (!t) {
            print("malloc failed\n");
            return;
        }

        if (curline->len != 0)
            memcpy(t, curline->text, curline->len * sizeof(char32_t));

        memcpy(t + curline->len, &c, sizeof(char32_t));

        if (curline->text)
            free(curline->text);

        curline->text = t;
        curline->len++;

        if (!no_move)
            cursor_x = (unsigned int)curline->len;
    }

    // lay out using harfbuzz

    do {
        // FIXME - keep HB buffer around?
        // FIXME - cap how much data we send to hb_buffer_add_utf8?

        unique_hb_buffer buf{hb_buffer_create()};

        hb_buffer_add_codepoints(buf.get(), (const hb_codepoint_t*)curline->text,
                                (int)curline->len, 0, -1);
        hb_buffer_guess_segment_properties(buf.get());

        hb_shape(font.hb_font, buf.get(), nullptr, 0);

        auto len = hb_buffer_get_length(buf.get());
        auto info = hb_buffer_get_glyph_infos(buf.get(), nullptr);
        auto pos = hb_buffer_get_glyph_positions(buf.get(), nullptr);

        curline->num_cells = min(len, console_fb_inst.console_cols);
        for (unsigned int i = 0; i < curline->num_cells; i++) {
            curline->cells[i].pos = pos[i];
            curline->cells[i].glyph = info[i].codepoint;

            if (i >= orig_cursor_x) {
                curline->cells[i].fg_colour = fg_colour;
                curline->cells[i].bg_colour = bg_colour;
                curline->cells[i].bold = bold ? 1 : 0;
                curline->cells[i].italic = italic ? 1 : 0;
            }
        }

        curline->changed = true;

        if (len <= console_fb_inst.console_cols)
            break;

        auto trunc = info[console_fb_inst.console_cols].cluster;

        auto t = (char32_t*)malloc((curline->len - trunc) * sizeof(char32_t));
        if (!t) {
            print("malloc failed\n");
            return;
        }

        memcpy(t, curline->text + trunc, (curline->len - trunc) * sizeof(char32_t));

        auto oldline = curline;

        console_overflow();

        curline->text = t;
        curline->len = oldline->len - trunc;
        curline->changed = true;

        oldline->text = (char32_t*)realloc(oldline->text, trunc * sizeof(char32_t));
        oldline->len = trunc;

        orig_cursor_x = 0;
    } while (true);
}

static void recv_data_fb(string_view s) {
    do_print(console_fb_inst, s);

    // print to FB

    unsigned int y = 0;

    for (auto le = lines.next; le != &lines; le = le->next) {
        auto& l = containing_record<console_line>(le);

        if (l.changed) {
            print_line(l, y);
            l.changed = true;
        }

        y++;
    }
}

static bool run_program(u16string_view exe, u16string_view cmdline,
                        unique_handle& proc, HANDLE client_handle) {
    HANDLE h, stdinh = 0, stdouth, curdirh = 0;
    unique_handle file, sect, thread;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    SECTION_IMAGE_INFORMATION sii;
    PROCESS_BASIC_INFORMATION pbi;
    RTL_USER_PROCESS_PARAMETERS params;
    PEB newpeb;
    INITIAL_TEB initial_teb;
    CONTEXT context;
    CLIENT_ID client_id;
    nt_status status;
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;

    // open file

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(exe.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)exe.data();

    status = NtOpenFile(&h, FILE_READ_DATA, &oa, &iosb, 0,
                        FILE_NON_DIRECTORY_FILE);
    if (!success(status)) {
        print("NtOpenFile returned {}\n", status);
        return false;
    }

    // FIXME - if not found, search PATH, etc.

    file.reset(h);

    // create section

    status = NtCreateSection(&h, 0, nullptr, nullptr, PAGE_READONLY, SEC_IMAGE,
                             file.get());
    if (!success(status)) {
        print("NtCreateSection returned {}\n", status);
        return false;
    }

    sect.reset(h);

    status = NtQuerySection(sect.get(), SECTION_INFORMATION_CLASS::ImageInformation,
                            &sii, sizeof(SECTION_IMAGE_INFORMATION), nullptr);

    if (!success(status)) {
        print("NtQuerySection returned {}\n", status);
        return false;
    }

    if (sii.ImageCharacteristics & IMAGE_FILE_DLL) {
        print("{}: not an executable\n", exe);
        return false;
    }

    if (sii.Machine != IMAGE_FILE_MACHINE_AMD64) {
        print("{}: unsupported architecture\n", exe);
        return false;
    }

    // create process

    status = NtCreateProcess(&h, MAXIMUM_ALLOWED, nullptr, NtCurrentProcess(),
                             false, sect.get(), 0, 0);
    if (!success(status)) {
        print("NtCreateProcess returned {}\n", status);
        return false;
    }

    proc.reset(h);

    // duplicate handles

    status = NtDuplicateObject(NtCurrentProcess(), client_handle,
                               proc.get(), &stdinh, FILE_READ_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);

    if (!success(status)) {
        print("NtDuplicateObject returned {}\n", status);
        return false;
    }

    status = NtDuplicateObject(NtCurrentProcess(), client_handle,
                               proc.get(), &stdouth, FILE_WRITE_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);

    if (!success(status)) {
        print("NtDuplicateObject returned {}\n", status);
        return false;
    }

    if (peb->ProcessParameters->CurrentDirectory.Handle) {
        status = NtDuplicateObject(NtCurrentProcess(), peb->ProcessParameters->CurrentDirectory.Handle,
                                   proc.get(), &curdirh, 0, 0,
                                   DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);

        if (!success(status)) {
            print("NtDuplicateObject returned {}\n", status);
            return false;
        }
    }

    // take copy of PEB

    status = NtQueryInformationProcess(proc.get(), PROCESSINFOCLASS::BasicInformation,
                                       &pbi, sizeof(pbi), nullptr);
    if (!success(status)) {
        print("NtQueryInformationProcess returned {}\n", status);
        return false;
    }

    status = NtReadVirtualMemory(proc.get(), pbi.PebBaseAddress, &newpeb,
                                 sizeof(PEB), nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return false;
    }

    // take copy of ProcessParameters

    status = NtReadVirtualMemory(proc.get(), newpeb.ProcessParameters, &params,
                                 sizeof(params), nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return false;
    }

    // change handles

    params.StandardInput = stdinh;
    params.StandardOutput = stdouth;
    params.StandardError = stdouth;
    params.CurrentDirectory.Handle = curdirh;

    // copy current directory string

    params.CurrentDirectory.DosPath.Length = min(params.CurrentDirectory.DosPath.MaximumLength,
                                                 peb->ProcessParameters->CurrentDirectory.DosPath.Length);

    status = NtWriteVirtualMemory(proc.get(), params.CurrentDirectory.DosPath.Buffer,
                                  peb->ProcessParameters->CurrentDirectory.DosPath.Buffer,
                                  params.CurrentDirectory.DosPath.Length, nullptr);
    if (!success(status)) {
        print("NtWriteVirtualMemory returned {}\n", status);
        return false;
    }

    // FIXME - ImagePathName
    // FIXME - environment variables

    // allocate misc block

    size_t misc_block_size = (cmdline.size() + 1) * sizeof(char16_t);

    auto misc_block = (uint8_t*)malloc(misc_block_size);
    if (!misc_block) {
        print("malloc failed\n");
        return false;
    }

    // allocate misc block in process

    void* proc_misc_block = nullptr;
    size_t proc_misc_block_size = bytes_to_pages(misc_block_size) << PAGE_SHIFT;

    status = NtAllocateVirtualMemory(proc.get(), &proc_misc_block, 0,
                                     &proc_misc_block_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status)) {
        print("NtAllocateVirtualMemory returned {}\n", status);
        free(misc_block);
        return false;
    }

    // copy cmdline

    memcpy(misc_block, cmdline.data(), cmdline.size() * sizeof(char16_t));
    misc_block[cmdline.size() * sizeof(char16_t)] = 0;
    params.CommandLine.Length = (uint16_t)(cmdline.size() * sizeof(char16_t));
    params.CommandLine.MaximumLength = (uint16_t)((cmdline.size() + 1) * sizeof(char16_t));
    params.CommandLine.Buffer = (char16_t*)proc_misc_block;

    // copy misc block

    status = NtWriteVirtualMemory(proc.get(), proc_misc_block, misc_block,
                                  (uint32_t)misc_block_size, nullptr);
    if (!success(status)) {
        print("NtWriteVirtualMemory returned {}\n", status);
        free(misc_block);
        return false;
    }

    free(misc_block);

    // write ProcessParameters back

    status = NtWriteVirtualMemory(proc.get(), newpeb.ProcessParameters, &params,
                                  sizeof(params), nullptr);
    if (!success(status)) {
        print("NtWriteVirtualMemory returned {}\n", status);
        return false;
    }

    // create thread

    memset(&initial_teb, 0, sizeof(initial_teb));

    void* stack = nullptr;
    size_t stack_size = 0x100000; // FIXME - should come from PE header?

    status = NtAllocateVirtualMemory(proc.get(), &stack, 0, &stack_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status)) {
        print("NtAllocateVirtualMemory returned {}\n", status);
        return false;
    }

    initial_teb.OldStackBase = nullptr;
    initial_teb.OldStackLimit = nullptr;
    initial_teb.StackBase = (uint8_t*)stack + stack_size;
    initial_teb.StackLimit = (uint8_t*)stack + stack_size;
    initial_teb.StackAllocationBase = stack;

    memset(&context, 0, sizeof(context));
    context.Rip = (uintptr_t)sii.TransferAddress;
    context.Rsp = (uintptr_t)initial_teb.StackBase - 0x20;
    context.EFlags = 0x200;
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;

    status = NtCreateThread(&h, THREAD_ALL_ACCESS, nullptr, proc.get(),
                            &client_id, &context, &initial_teb, true);
    if (!success(status)) {
        print("NtCreateThread returned {}\n", status);
        return false;
    }

    thread.reset(h);

    // FIXME - Ctrl+C handling

    NtResumeThread(thread.get(), nullptr);

    return true;
}

static void run_shell(u16string_view exe, u16string_view cmdline,
                      void (*recv_func)(string_view), void (*flush_func)()) {
    HANDLE h;
    unique_handle server_handle, client_handle, proc, kmsg, keyboard_pipe;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    PROCESS_BASIC_INFORMATION pbi;
    char16_t pipename[50];

    // open kmsg

    static const u16string_view kmsg_fn = u"\\dev\\kmsg";
    static const u16string_view keyboard_pipe_fn = u"\\dev\\keyboard";

    us.Length = us.MaximumLength = (uint16_t)(kmsg_fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)kmsg_fn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtOpenFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa, &iosb, 0, 0);
    if (!success(status)) {
        print("NtOpenFile returned {}\n", status);
        return;
    }

    kmsg.reset(h);

    // create pipe

    status = NtQueryInformationProcess(NtCurrentProcess(),
                                       PROCESSINFOCLASS::BasicInformation,
                                       &pbi, sizeof(pbi), nullptr);
    if (!success(status)) {
        print("NtQueryInformationProcess returned {}\n", status);
        return;
    }

    {
        char s[50];
        span sp(s, sizeof(s));

        sp = format(sp, "\\dev\\userconpipe-{}", pbi.UniqueProcessId);

        auto name = string_view(s, (size_t)(sp.data() - s));

        utf8_to_utf16(pipename, name);

        us.Length = us.MaximumLength = (uint16_t)(utf8_to_utf16_len(name) * sizeof(char16_t));
        us.Buffer = pipename;
    }

    status = NtCreateNamedPipeFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa,
                                   &iosb, 0, FILE_CREATE, 0, 1, 0, 0, 0, 0, 0,
                                   nullptr);
    if (!success(status)) {
        print("NtCreateNamedPipeFile returned {}\n", status);
        return;
    }

    server_handle.reset(h);

    status = NtOpenFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa, &iosb, 0, 0);
    if (!success(status)) {
        print("NtOpenFile returned {}\n", status);
        return;
    }

    client_handle.reset(h);

    us.Length = us.MaximumLength = (uint16_t)(keyboard_pipe_fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)keyboard_pipe_fn.data();

    status = NtOpenFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa, &iosb, 0, 0);
    if (!success(status)) {
        print("NtOpenFile returned {}\n", status);
        return;
    }

    keyboard_pipe.reset(h);

    // print pending kmsg buffer first

    while (true) {
        LARGE_INTEGER timeout;
        char s[PAGE_SIZE];

        timeout.QuadPart = 0;

        status = NtWaitForSingleObject(kmsg.get(), true, &timeout);
        if (status == nt_status::ALERTED)
            continue;
        else if (status == nt_status::TIMEOUT)
            break;
        else if (!success(status)) {
            print("NtWaitForSingleObject returned {}\n", status);
            return;
        }

        status = NtReadFile(kmsg.get(), 0, nullptr, nullptr,
                            &iosb, s, sizeof(s), nullptr, nullptr);
        if (status == nt_status::ALERTED)
            continue;
        else if (!success(status)) {
            print("NtReadFile returned {}\n", status);
            return;
        }

        if (iosb.Information != 0)
            recv_func(string_view(s, iosb.Information));
    }

    recv_func(welcome_message);
    flush_func();

    if (!run_program(exe, cmdline, proc, client_handle.get()))
        return;

    do {
        array<HANDLE, 4> objs;

        objs[0] = kmsg.get();
        objs[1] = keyboard_pipe.get();
        objs[2] = server_handle.get();
        objs[3] = proc.get();

        status = NtWaitForMultipleObjects(objs.size(), objs.data(), true,
                                          true, nullptr);
        if (status == nt_status::ALERTED)
            continue;

        if (!success(status)) {
            print("NtWaitForMultipleObjects returned {}\n", status);
            return;
        }

        switch ((unsigned int)status - (unsigned int)nt_status::SUCCESS) {
            case 0: {
                char s[PAGE_SIZE];

                // print kmsg data

                status = NtReadFile(kmsg.get(), 0, nullptr, nullptr,
                                    &iosb, s, sizeof(s), nullptr, nullptr);
                if (status == nt_status::ALERTED)
                    continue;
                else if (!success(status)) {
                    print("NtReadFile returned {}\n", status);
                    return;
                }

                if (iosb.Information != 0) {
                    recv_func(string_view(s, iosb.Information));
                    flush_func();
                }

                break;
            }

            case 1: {
                input_event_with_id ev;

                status = NtReadFile(keyboard_pipe.get(), 0, nullptr, nullptr,
                                    &iosb, &ev, sizeof(ev), nullptr, nullptr);

                if (status == nt_status::ALERTED)
                    continue;
                else if (!success(status)) {
                    print("NtReadFile returned {}\n", status);
                    return;
                }

                auto sp = handle_keyboard_event(ev, keyboard_pipe.get());

                // FIXME - only look for Ctrl+C if ENABLE_PROCESSED_INPUT set

                if (sp.size() == 1 && sp[0] == 0x03) // Ctrl+C
                    send_ctrl_c(proc.get());
                else if (!sp.empty()) {
                    IO_STATUS_BLOCK iosb;

                    while (!sp.empty()) {
                        auto status = NtWriteFile(server_handle.get(), 0, nullptr,
                                                  nullptr, &iosb, sp.data(),
                                                  (uint32_t)sp.size(), nullptr,
                                                  nullptr);
                        if (status == nt_status::ALERTED)
                            continue;
                        else if (!success(status)) {
                            print("NtWriteFile returned {}\n", status);
                            return;
                        }

                        if (iosb.Information >= sp.size())
                            break;

                        sp = span(sp.data() + iosb.Information,
                                  sp.size() - iosb.Information);
                    }
                }

                break;
            }

            case 2: {
                do {
                    char s[PAGE_SIZE];
                    LARGE_INTEGER timeout;

                    // print pipe data

                    status = NtReadFile(server_handle.get(), 0, nullptr, nullptr,
                                        &iosb, s, sizeof(s), nullptr, nullptr);
                    if (status == nt_status::ALERTED)
                        break;
                    else if (!success(status)) {
                        print("NtReadFile returned {}\n", status);
                        return;
                    }

                    recv_func(string_view(s, iosb.Information));

                    timeout.QuadPart = 0;

                    // go again if still data pending
                    status = NtWaitForSingleObject(server_handle.get(), true,
                                                   &timeout);
                    if (status == nt_status::ALERTED || status == nt_status::TIMEOUT)
                        break;
                    else if (!success(status)) {
                        print("NtWaitForSingleObject returned {}\n", status);
                        return;
                    }
                } while (false);

                flush_func();

                if (status == nt_status::ALERTED)
                    continue;

                break;
            }

            case 3: {
                // process terminated, start again

                if (!run_program(exe, cmdline, proc, client_handle.get()))
                    return;

                break;
            }


        }
    } while (true);
}

static int try_text_console() {
    text_console = init_text_console();
    if (text_console.empty()) {
        print("init_text_console failed\n");
        return 1;
    }

    run_shell(u"\\shell.exe", u"shell.exe", recv_data_text, flush_text_console);

    return 0;
}

static int do_console() {
    bool no_framebuffer = false;

    framebuffer = init_framebuffer(no_framebuffer);
    if (no_framebuffer)
        return try_text_console();
    else if (framebuffer.empty()) {
        print("init_framebuffer failed\n");
        return 1;
    }

    auto error = FT_Init_FreeType(&ft); // FIXME - free at end
    if (error) {
        print("FT_Init_FreeType failed ({:x})\n", error);
        return 1;
    }

    auto fontbuf = load_font(u"console.ttf");
    if (fontbuf.empty()) {
        print("load_font failed\n");
        return 1;
    }

    auto boldfontbuf = load_font(u"console-bold.ttf");
    if (boldfontbuf.empty()) {
        print("load_font failed\n");
        return 1;
    }

    auto italicfontbuf = load_font(u"console-italic.ttf");
    if (italicfontbuf.empty()) {
        print("load_font failed\n");
        return 1;
    }

    auto bolditalicfontbuf = load_font(u"console-bold-italic.ttf");
    if (italicfontbuf.empty()) {
        print("load_font failed\n");
        return 1;
    }

    if (!init_font(fontbuf, font, true)) {
        print("init_font failed\n");
        return 1;
    }

    if (!init_font(boldfontbuf, bold_font, false)) {
        print("init_font failed\n");
        return 1;
    }

    if (!init_font(italicfontbuf, italic_font, false)) {
        print("init_font failed\n");
        return 1;
    }

    if (!init_font(bolditalicfontbuf, bold_italic_font, false)) {
        print("init_font failed\n");
        return 1;
    }

    rect(0, 0, fb_info.width, fb_info.height, 0x000000);
    flush_framebuffer();

    run_shell(u"\\shell.exe", u"shell.exe", recv_data_fb, flush_framebuffer);

    return 0;
}

static void serial_print(string_view s) {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    if (s.size() == 0)
        return;

    while (true) {
        auto status = NtWriteFile(peb->ProcessParameters->StandardOutput, 0,
                                  nullptr, nullptr, &iosb, s.data(),
                                  (uint32_t)s.size(), nullptr, nullptr);
        if (!success(status)) {
            print("NtWriteFile returned {}\n", status);
            return;
        }

        if (iosb.Information == s.size())
            break;

        s = string_view(s.data() + iosb.Information, s.size() - iosb.Information);
    }
}

static void handle_serial_stdin(span<const char> s, HANDLE proc,
                                invocable<span<char>> auto& func) {
    // FIXME - only look for Ctrl+C etc. if ENABLE_PROCESSED_INPUT set

    // handle VT100 weirdness

    int last_run_start = -1;

    for (size_t i = 0; i < s.size(); i++) {
        auto c = s[i];

        if (c == '\r' || c == 0x7f || c == '\x03') {
            if (last_run_start == -1)
                func(s.subspan(0, (size_t)i));
            else
                func(s.subspan((size_t)(last_run_start + 1), i - (size_t)(last_run_start + 1)));

            if (c == '\r') {
                c = '\n';
                func(span(&c, 1));
            } else if (c == 0x7f) {
                c = '\b';
                func(span(&c, 1));
            } else if (c == '\x03') {
                EXCEPTION_RECORD er;

                er.ExceptionFlags = 0;
                er.ExceptionRecord = nullptr;
                er.ExceptionCode = (uint32_t)nt_status::CONTROL_C_EXIT;
                er.NumberParameters = 0;

                send_signal_to_process(proc, er);
            }

            last_run_start = (int)i;
        }
    }

    func(s.subspan((size_t)(last_run_start + 1), s.size() - (size_t)(last_run_start + 1)));
}

static int do_serial_console() {
    HANDLE h;
    unique_handle server_handle, client_handle, proc;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    PROCESS_BASIC_INFORMATION pbi;
    char16_t pipename[50];
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;

    static const u16string_view exe = u"\\shell.exe";
    static const u16string_view cmdline = u"shell.exe";

    // create pipe

    status = NtQueryInformationProcess(NtCurrentProcess(),
                                       PROCESSINFOCLASS::BasicInformation,
                                       &pbi, sizeof(pbi), nullptr);
    if (!success(status)) {
        print("NtQueryInformationProcess returned {}\n", status);
        return 1;
    }

    {
        char s[50];
        span sp(s, sizeof(s));

        sp = format(sp, "\\dev\\userconpipe-{}", pbi.UniqueProcessId);

        auto name = string_view(s, (size_t)(sp.data() - s));

        utf8_to_utf16(pipename, name);

        us.Length = us.MaximumLength = (uint16_t)(utf8_to_utf16_len(name) * sizeof(char16_t));
        us.Buffer = pipename;
    }

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtCreateNamedPipeFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa,
                                   &iosb, 0, FILE_CREATE, 0, 1, 0, 0, 0, 0, 0,
                                   nullptr);
    if (!success(status)) {
        print("NtCreateNamedPipeFile returned {}\n", status);
        return 1;
    }

    server_handle.reset(h);

    status = NtOpenFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa, &iosb, 0, 0);
    if (!success(status)) {
        print("NtOpenFile returned {}\n", status);
        return 1;
    }

    client_handle.reset(h);

    serial_print(welcome_message);

    if (!run_program(exe, cmdline, proc, client_handle.get()))
        return 1;

    do {
        array<HANDLE, 3> objs;

        objs[0] = peb->ProcessParameters->StandardInput;
        objs[1] = server_handle.get();
        objs[2] = proc.get();

        status = NtWaitForMultipleObjects(objs.size(), objs.data(), true,
                                          true, nullptr);
        if (status == nt_status::ALERTED)
            continue;

        if (!success(status)) {
            print("NtWaitForMultipleObjects returned {}\n", status);
            return 1;
        }

        switch ((unsigned int)status - (unsigned int)nt_status::SUCCESS) {
            case 0: {
                char buf[1024];

                // forward data on stdin

                status = NtReadFile(peb->ProcessParameters->StandardInput, 0,
                                    nullptr, nullptr, &iosb, buf, sizeof(buf),
                                    nullptr, nullptr);

                if (status == nt_status::ALERTED)
                    continue;
                else if (!success(status)) {
                    print("NtReadFile returned {}\n", status);
                    return 1;
                }

                span read(buf, iosb.Information);

                auto do_write = [&server_handle](span<const char> buf) {
                    IO_STATUS_BLOCK iosb;

                    while (!buf.empty()) {
                        auto status = NtWriteFile(server_handle.get(), 0, nullptr,
                                                  nullptr, &iosb, buf.data(),
                                                  (uint32_t)buf.size(),
                                                  nullptr, nullptr);
                        if (status == nt_status::ALERTED)
                            continue;
                        else if (!success(status)) {
                            print("NtWriteFile returned {}\n", status);
                            return;
                        }

                        if (iosb.Information >= buf.size())
                            break;

                        buf = string_view(buf.data() + iosb.Information,
                                          buf.size() - iosb.Information);
                    }
                };

                handle_serial_stdin(read, proc.get(), do_write);

                break;
            }

            case 1: {
                char s[PAGE_SIZE];

                // print pipe data

                status = NtReadFile(server_handle.get(), 0, nullptr, nullptr,
                                    &iosb, s, sizeof(s), nullptr, nullptr);
                if (status == nt_status::ALERTED)
                    continue;
                else if (!success(status)) {
                    print("NtReadFile returned {}\n", status);
                    return 1;
                }

                serial_print(string_view(s, iosb.Information));
                break;
            }

            case 2: {
                // process terminated, start again

                if (!run_program(exe, cmdline, proc, client_handle.get()))
                    return 1;

                break;
            }
        }
    } while (true);

    return 0;
}

static int _main() {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    int ret;
    bool serial_mode = false;

    // FIXME - serial version

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

    if (!argv.empty())
        argv = argv.subspan(1);

    for (auto arg : argv) {
        if (arg.empty())
            continue;

        if (arg == u"serial")
            serial_mode = true;
        else {
            print("unhandled command line option \"{}\"\n", arg);
            RtlFreeHeap(GetProcessHeap(), 0, buf);
            return 1;
        }
    }

    if (serial_mode)
        ret = do_serial_console();
    else
        ret = do_console();

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    return ret;
}

static void call_global_constructors() {
    auto v = &__CTOR_LIST__;

    if (*v != 0xffffffffffffffff)
        return;

    v++;

    while (*v != 0) {
        auto func = reinterpret_cast<void (*)()>(*v);

        func();

        v++;
    }
}

extern "C"
__attribute__((force_align_arg_pointer)) void startup() {
    call_global_constructors();

    NtTerminateThread(NtCurrentThread(), (NTSTATUS)_main());
}
