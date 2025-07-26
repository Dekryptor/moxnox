module;

#include <nt.h>
#include <span>
#include <string_view>
#include <string.h>

export module text;

import cp437;
import vt100;
import print;

using namespace std;

export span<uint16_t> text_console;
export MN_SECTION_TEXT_CONSOLE_INFORMATION tc_info;

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

struct console_text {
    void console_overflow(unsigned int rows = 1);
    static void handle_sgr_part_num(unsigned int num);
    void print_char(char32_t c, bool no_move);
    void cursor_changed();
    void scroll_down(unsigned int rows);
    void enable_cursor(bool enable);
    void move_cursor();

    unsigned int cursor_x, cursor_y;
    unsigned int console_rows, console_cols;
};

export console_text console_text_inst;

static const auto DEFAULT_BG_COLOUR_TEXT = console_colour::black;
static const auto DEFAULT_FG_COLOUR_TEXT = console_colour::light_grey;
static enum console_colour bg_colour_text = DEFAULT_BG_COLOUR_TEXT;
static enum console_colour fg_colour_text = DEFAULT_FG_COLOUR_TEXT;

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
    // FIXME
}

void console_text::handle_sgr_part_num(unsigned int num) {
    // FIXME
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

void console_text::cursor_changed() {
    // FIXME
}

void console_text::scroll_down(unsigned int rows) {
    // FIXME
}

void console_text::enable_cursor(bool enable) {
    // FIXME
}

void console_text::move_cursor() {
    // FIXME
}

export void recv_data_text(string_view s) {
    do_print(console_text_inst, s);

    // print to FB
//
//     unsigned int y = 0;
//
//     for (auto le = lines.next; le != &lines; le = le->next) {
//         auto& l = containing_record<console_line>(le);
//
//         if (l.changed) {
//             print_line(l, y);
//             l.changed = true;
//         }
//
//         y++;
//     }
//
    flush_text_console();
}

export span<uint16_t> init_text_console() {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE sect;
    void* addr;
    size_t size, return_length;

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

    NtClose(sect);

    return span((uint16_t*)addr, size / sizeof(uint16_t));
}
