module;

#include <nt.h>
#include <ioctl.h>
#include <span>
#include <string_view>
#include <string.h>

export module text;

import cp437;
import vt100;
import print;

using namespace std;

export span<uint16_t> text_console;

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

struct console_text {
    void console_overflow(unsigned int rows = 1);
    static void handle_sgr_part_num(unsigned int num);
    void print_char(char32_t c, bool no_move);
    void cursor_changed() { }
    void scroll_down(unsigned int rows);
    void enable_cursor(bool enable);
    void move_cursor();

    // FIXME - would need atexit
    // ~console_text() {
    //     if (section != 0)
    //         NtClose(section);
    // }

    unsigned int cursor_x = 0, cursor_y = 0;
    unsigned int console_rows, console_cols;
    HANDLE section = 0;
};

export console_text console_text_inst;

static const unsigned int DEFAULT_BG_COLOUR_INDEX = 0;
static const unsigned int DEFAULT_FG_COLOUR_INDEX = 7;
static uint32_t bg_colour_index = 0, fg_colour_index = 7;
static const auto DEFAULT_BG_COLOUR_TEXT = console_colour::black;
static const auto DEFAULT_FG_COLOUR_TEXT = console_colour::light_grey;
static enum console_colour bg_colour_text = DEFAULT_BG_COLOUR_TEXT;
static enum console_colour fg_colour_text = DEFAULT_FG_COLOUR_TEXT;
static bool bg_bright = false, fg_bright = false, bold = false;

static void flush_text_console() {
    void* flush_addr = text_console.data();
    size_t flush_size = text_console.size_bytes();
    IO_STATUS_BLOCK iosb;

    auto status = NtFlushVirtualMemory(NtCurrentProcess(), &flush_addr,
                                       &flush_size, &iosb);
    if (!success(status)) {
        print("NtFlushVirtualMemory returned {}\n", status);
        return;
    }
}

static constexpr uint16_t text_console_val(uint8_t c, enum console_colour bg,
                                           enum console_colour fg) {
    return (uint16_t)(((uint16_t)bg << 12) | ((uint16_t)fg << 8) | c);
}

static void text_console_putc(uint8_t x, uint8_t y, char32_t cp,
                              enum console_colour bg, enum console_colour fg) {
    auto c = cp437(cp);

    text_console[(y * console_text_inst.console_cols) + x] = text_console_val((uint8_t)c, bg, fg);
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

void console_text::print_char(char32_t c, bool no_move) {
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
        if (cursor_x > 0) { // FIXME
            cursor_x--;

            // FIXME - only if console_output_mode & ENABLE_PROCESSED_OUTPUT
            text_console_putc((uint8_t)cursor_x, (uint8_t)cursor_y, ' ',
                              bg_colour_text, fg_colour_text);
        }
    } else {
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

void console_text::scroll_down(unsigned int rows) {
    memmove(&text_console[console_cols * rows], text_console.data(),
            (console_rows - rows) * console_cols * sizeof(uint16_t));

    for (uint8_t y = 0; y < rows; y++) {
        for (uint8_t x = 0; x < console_cols; x++) {
            text_console_putc(x, y, ' ', bg_colour_text, fg_colour_text);
        }
    }
}

void console_text::enable_cursor(bool enable) {
    // FIXME
}

void console_text::move_cursor() {
    IO_STATUS_BLOCK iosb;
    MOXNOX_SET_CURSOR mnsc;

    mnsc.x = cursor_x;
    mnsc.y = cursor_y;

    auto status = NtDeviceIoControlFile(section, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_CONSOLE_SET_CURSOR, &mnsc,
                                        sizeof(mnsc), nullptr, 0);
    if (!success(status)) {
        print("IOCTL_MOXNOX_CONSOLE_SET_CURSOR returned {}\n", status);
        return;
    }
}

export void recv_data_text(string_view s) {
    do_print(console_text_inst, s);

    flush_text_console();
}

export span<uint16_t> init_text_console() {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE sect;
    void* addr;
    size_t size, return_length;
    MN_SECTION_TEXT_CONSOLE_INFORMATION tc_info;

    static const u16string_view fn = u"\\dev\\text_console";

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

    status = NtQuerySection(sect, SECTION_INFORMATION_CLASS::MoxnoxTextConsoleInformation,
                            &tc_info, sizeof(tc_info), &return_length);
    if (!success(status)) {
        print("NtQuerySection returned {}\n", status);
        NtClose(sect);
        return {};
    }

    console_text_inst.console_cols = tc_info.cols;
    console_text_inst.console_rows = tc_info.rows;

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

    console_text_inst.section = sect;

    return span((uint16_t*)addr, size / sizeof(uint16_t));
}
