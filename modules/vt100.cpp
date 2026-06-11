module;

#include <stdint.h>
#include <stdlib.h>
#include <string_view>
#include <span>
#include <array>
#include <type_traits>
#include <algorithm>

export module vt100;

import format;

using namespace std;

enum class esc_state {
    normal,
    got_esc,
    in_csi,
    in_designate,
    in_osc,
};

uint8_t utf8_buf[5];
unsigned int utf8_chars = 0;
char escape_buffer[50];
size_t escape_buffer_len = 0;
esc_state esc_state = esc_state::normal;
bool using_acs = false;
char32_t last_char = 0;

unsigned int saved_cursor_x, saved_cursor_y;
bool saved_using_acs = false;

export template<typename T>
concept vt100_console = requires(T t) {
    requires unsigned_integral<decltype(t.cursor_x)>;
    requires unsigned_integral<decltype(t.cursor_y)>;
    requires unsigned_integral<decltype(t.console_rows)>;
    requires unsigned_integral<decltype(t.console_cols)>;
    requires unsigned_integral<decltype(t.scroll_top)>;
    requires unsigned_integral<decltype(t.scroll_bottom)>;
    requires same_as<decltype(t.decckm), bool>;
    requires same_as<decltype(t.auto_wrap), bool>;
    requires same_as<decltype(t.insert_mode), bool>;
    { t.console_overflow((unsigned int)0) } -> same_as<void>;
    { t.scroll_down((unsigned int)0) } -> same_as<void>;
    { t.handle_sgr_part_num((unsigned int)0) } -> same_as<void>;
    { t.cursor_changed() } -> same_as<void>;
    { t.move_cursor() } -> same_as<void>;
    { t.print_chars(span<const char32_t>()) } -> same_as<void>;
    { t.enable_cursor(false) } -> same_as<void>;
    { t.erase_in_line((unsigned int)0) } -> same_as<void>;
    { t.erase_in_display((unsigned int)0) } -> same_as<void>;
    { t.delete_chars((unsigned int)0) } -> same_as<void>;
    { t.erase_chars((unsigned int)0) } -> same_as<void>;
    { t.insert_chars((unsigned int)0) } -> same_as<void>;
    { t.insert_lines((unsigned int)0) } -> same_as<void>;
    { t.delete_lines((unsigned int)0) } -> same_as<void>;
    { t.enter_alt_screen() } -> same_as<void>;
    { t.exit_alt_screen() } -> same_as<void>;
    { t.write_input_data("") } -> same_as<void>;
    { t.get_bg_colour() } -> same_as<uint32_t>;
};

export template<typename T>
concept vt100_console_with_tab_stops = requires(T& t) {
    requires vt100_console<T>;
    requires same_as<decltype(t.tab_stops), bool*>;
    requires unsigned_integral<decltype(t.tab_stops_count)>;
};

const char32_t ACS_CODES_START = 0x5f;

const array<char32_t, 32> acs_codes = {
    0x00a0,
    0x25c6,
    0x2592,
    0x2409,
    0x240c,
    0x240d,
    0x240a,
    0x00b0,
    0x00b1,
    0x2424,
    0x240b,
    0x2518,
    0x2510,
    0x250c,
    0x2514,
    0x253c,
    0x23ba,
    0x23bb,
    0x2500,
    0x23bc,
    0x23bd,
    0x251c,
    0x2524,
    0x2534,
    0x252c,
    0x2502,
    0x2264,
    0x2265,
    0x03c0,
    0x2260,
    0x00a3,
    0x00b7,
};

void print_chars(vt100_console auto& t, span<const char32_t> s) {
    t.print_chars(s);
    t.cursor_changed();

    last_char = s.back();
}

void do_print2(vt100_console auto& t, span<const char> s) {
    char32_t buf[100];
    span<char32_t> sp(buf);

    for (size_t i = 0; i < s.size(); i++) {
        auto c = s[i];

        if (sp.empty()) {
            print_chars(t, buf);
            sp = buf;
        }

        if (utf8_chars != 0) {
            if (!(c & 0x80) || utf8_chars == sizeof(utf8_buf)) {
                if (sp.size() < 2) {
                    print_chars(t, span(buf, (sizeof(buf) / sizeof(buf[0])) - sp.size()));
                    sp = buf;
                }

                sp[0] = 0xfffd;
                sp[1] = (char32_t)c;
                sp = span(sp.data() + 2, sp.size() - 2);

                utf8_chars = 0;
                continue;
            }

            utf8_buf[utf8_chars] = (uint8_t)c;
            utf8_chars++;

            if ((utf8_buf[0] & 0xe0) == 0xc0 && utf8_chars == 2) {
                if ((utf8_buf[1] & 0xc0) != 0x80) {
                    sp[0] = 0xfffd;
                    sp = span(sp.data() + 1, sp.size() - 1);

                    utf8_chars = 0;
                    continue;
                }

                auto c2 = (char32_t)(((utf8_buf[0] & 0x1f) << 6) | (utf8_buf[1] & 0x3f));

                sp[0] = c2;
                sp = span(sp.data() + 1, sp.size() - 1);

                utf8_chars = 0;
            } else if ((utf8_buf[0] & 0xf0) == 0xe0) {
                if (utf8_chars == 2) {
                    if ((utf8_buf[1] & 0xc0) != 0x80) {
                        sp[0] = 0xfffd;
                        sp = span(sp.data() + 1, sp.size() - 1);

                        utf8_chars = 0;
                    }
                } else {
                    if ((utf8_buf[2] & 0xc0) != 0x80) {
                        sp[0] = 0xfffd;
                        sp = span(sp.data() + 1, sp.size() - 1);

                        utf8_chars = 0;
                        continue;
                    }

                    auto c2 = (char32_t)(((utf8_buf[0] & 0xf) << 12) | ((utf8_buf[1] & 0x3f) << 6) | (utf8_buf[2] & 0x3f));

                    sp[0] = c2;
                    sp = span(sp.data() + 1, sp.size() - 1);

                    utf8_chars = 0;
                }
            } else if ((utf8_buf[0] & 0xf8) == 0xf0) {
                if (utf8_chars == 2) {
                    if ((utf8_buf[1] & 0xc0) != 0x80) {
                        sp[0] = 0xfffd;
                        sp = span(sp.data() + 1, sp.size() - 1);

                        utf8_chars = 0;
                    }
                } else if (utf8_chars == 3) {
                    if ((utf8_buf[2] & 0xc0) != 0x80) {
                        sp[0] = 0xfffd;
                        sp = span(sp.data() + 1, sp.size() - 1);

                        utf8_chars = 0;
                    }
                } else {
                    if ((utf8_buf[3] & 0xc0) != 0x80) {
                        sp[0] = 0xfffd;
                        sp = span(sp.data() + 1, sp.size() - 1);

                        utf8_chars = 0;
                        continue;
                    }

                    auto c2 = (char32_t)(((utf8_buf[0] & 0x7) << 18) | ((utf8_buf[1] & 0x3f) << 12) | ((utf8_buf[2] & 0x3f) << 6) | (utf8_buf[3] & 0x3f));

                    sp[0] = c2;
                    sp = span(sp.data() + 1, sp.size() - 1);

                    utf8_chars = 0;
                }
            }
        } else if ((c & 0xc0) == 0xc0) { // first character of UTF-8 sequence
            utf8_buf[0] = (uint8_t)c;
            utf8_chars = 1;
        } else if (c & 0x80) {
            sp[0] = 0xfffd;
            sp = span(sp.data() + 1, sp.size() - 1);
        } else if (c == '\t') {
            auto filled = (sizeof(buf) / sizeof(buf[0])) - sp.size();

            if (filled > 0) {
                print_chars(t, span(buf, filled));
                sp = span(buf, sizeof(buf) / sizeof(buf[0]));
            }

            if constexpr (vt100_console_with_tab_stops<remove_reference_t<decltype(t)>>) {
                auto new_x = t.cursor_x + 1;

                while (new_x < t.console_cols && !t.tab_stops[new_x]) {
                    new_x++;
                }

                t.cursor_x = min(new_x, t.console_cols - 1);
                t.cursor_changed();
            }
        } else {
            if (using_acs && (char32_t)c >= ACS_CODES_START && (char32_t)c < ACS_CODES_START + acs_codes.size())
                sp[0] = acs_codes[(char32_t)c - ACS_CODES_START];
            else
                sp[0] = (char32_t)c;

            sp = span(sp.data() + 1, sp.size() - 1);
        }
    }

    if (sp.size() < (sizeof(buf) / sizeof(buf[0])))
        print_chars(t, span(buf, (sizeof(buf) / sizeof(buf[0])) - sp.size()));
}

void handle_sgr_part(vt100_console auto& t, string_view s) {
    while (!s.empty() && s.front() == '0') {
        s = string_view(s.data() + 1, s.size() - 1);
    }

    unsigned int num = 0;

    for (auto c : s) {
        if (c < '0' || c > '9')
            return;

        num *= 10;
        num += (unsigned int)(c - '0');
    }

    t.handle_sgr_part_num(num);
}

void handle_sgr(vt100_console auto& t, string_view s) {
    do {
        string_view p;

        auto sc = s.find(';');

        if (sc == string_view::npos)
            p = s;
        else
            p = string_view(s.data(), sc);

        handle_sgr_part(t, p);

        if (sc == string_view::npos)
            break;

        s = string_view(s.data() + sc + 1, s.size() - sc - 1);
    } while (true);
}

void move_cursor(vt100_console auto& t, string_view s) {
    unsigned int param = 1;

    if (s.size() > 1) {
        param = 0;

        for (size_t i = 0; i < s.size() - 1; i++) {
            auto c = s[i];

            if (c < '0' || c > '9')
                return;

            param *= 10;
            param += (unsigned int)(c - '0');
        }
    }

    // old position
    t.cursor_changed();

    if (s.back() == 'A') { // up
        if (t.cursor_y > param - 1)
            t.cursor_y -= param;
        else
            t.cursor_y = 0;
    } else if (s.back() == 'B') { // down
        if (t.cursor_y < t.console_rows - param)
            t.cursor_y += param;
        else
            t.cursor_y = t.console_rows - 1;
    } else if (s.back() == 'C') { // right
        if (t.cursor_x < t.console_cols - param)
            t.cursor_x += param;
        else
            t.cursor_x = t.console_cols - 1;
    } else if (s.back() == 'D') { // left
        if (t.cursor_x > param - 1)
            t.cursor_x -= param;
    } else if (s.back() == 'E') { // cursor next line
        t.cursor_x = 0;

        if (t.cursor_y < t.console_rows - param)
            t.cursor_y += param;
        else
            t.cursor_y = t.console_rows - 1;
    } else if (s.back() == 'F') { // cursor previous line
        t.cursor_x = 0;

        if (t.cursor_y > param - 1)
            t.cursor_y -= param;
        else
            t.cursor_y = 0;
    } else if (s.back() == 'G') { // cursor horizontal absolute
        if (param < 1)
            param = 1;

        if (param > t.console_cols)
            t.cursor_x = t.console_cols - 1;
        else
            t.cursor_x = param - 1;
    } else if (s.back() == 'd') { // vertical position absolute
        if (param < 1)
            param = 1;

        if (param > t.console_rows)
            t.cursor_y = t.console_rows - 1;
        else
            t.cursor_y = param - 1;
    }

    // new position
    t.cursor_changed();
}

void cursor_backward_tab(vt100_console_with_tab_stops auto& t, string_view s) {
    unsigned int n = 0;

    if (!s.empty()) {
        for (auto c : s) {
            if (c < '0' || c > '9')
                break;

            n *= 10;
            n += (unsigned int)(c - '0');
        }
    }

    if (n == 0)
        n = 1;

    for (unsigned int i = 0; i < n; i++) {
        if (t.cursor_x == 0)
            break;

        unsigned int x = t.cursor_x - 1;

        while (x > 0 && !t.tab_stops[x]) {
            x--;
        }

        t.cursor_x = x;
    }

    t.cursor_changed();
}

void clear_all_tabs(vt100_console_with_tab_stops auto& t, string_view s) {
    unsigned int param = 0;

    for (auto c : s) {
        if (c < '0' || c > '9')
            break;

        param *= 10;
        param += (unsigned int)(c - '0');
    }

    if (param == 3) {
        for (unsigned int i = 0; i < t.tab_stops_count; i++) {
            t.tab_stops[i] = false;
        }
    } else {
        if (t.cursor_x < t.tab_stops_count)
            t.tab_stops[t.cursor_x] = false;
    }
}

void scroll_up(vt100_console auto& t, string_view s) {
    unsigned int rows = 1;

    if (!s.empty()) {
        rows = 0;

        for (auto c : s) {
            if (c < '0' || c > '9')
                return;

            rows *= 10;
            rows += (unsigned int)(c - '0');
        }

        if (rows == 0)
            return;
    }

    if (rows > t.console_rows)
        rows = t.console_rows;

    t.console_overflow(rows);
}

void set_cursor_position(vt100_console auto& t, string_view s) {
    unsigned int x = 1, y = 1;

    if (!s.empty()) {
        y = 0;

        while (!s.empty()) {
            if (s.front() == ';')
                break;

            if (s.front() < '0' || s.front() > '9')
                return;

            y *= 10;
            y += (unsigned int)(s.front() - '0');

            s = string_view(s.data() + 1, s.size() - 1);
        }

        if (!s.empty()) {
            if (s.front() == ';')
                s = string_view(s.data() + 1, s.size() - 1);

            if (!s.empty()) {
                x = 0;

                while (!s.empty()) {
                    if (s.front() < '0' || s.front() > '9')
                        return;

                    x *= 10;
                    x += (unsigned int)(s.front() - '0');

                    s = string_view(s.data() + 1, s.size() - 1);
                }
            }
        }
    }

    // old position
    t.cursor_changed();

    if (x == 0)
        x = 1;

    if (y == 0)
        y = 1;

    if (x >= t.console_cols)
        x = t.console_cols;

    if (y >= t.console_rows)
        y = t.console_rows;

    t.cursor_x = x - 1;
    t.cursor_y = y - 1;

    // new position
    t.cursor_changed();
}

void scroll_down(vt100_console auto& t, string_view s) {
    unsigned int rows = 1;

    if (!s.empty()) {
        rows = 0;

        for (auto c : s) {
            if (c < '0' || c > '9')
                return;

            rows *= 10;
            rows += (unsigned int)(c - '0');
        }

        if (rows == 0)
            return;
    }

    if (rows > t.console_rows)
        rows = t.console_rows;

    t.scroll_down(rows);
}

void erase_in_line(vt100_console auto& t, string_view s) {
    unsigned int param = 0;

    if (!s.empty()) {
        param = 0;

        for (auto c : s) {
            if (c < '0' || c > '9')
                return;

            param *= 10;
            param += (unsigned int)(c - '0');
        }
    }

    if (param > 2)
        return;

    t.erase_in_line(param);
}

void erase_in_display(vt100_console auto& t, string_view s) {
    unsigned int param = 0;

    if (!s.empty()) {
        param = 0;

        for (auto c : s) {
            if (c < '0' || c > '9')
                return;

            param *= 10;
            param += (unsigned int)(c - '0');
        }
    }

    if (param > 3)
        return;

    t.erase_in_display(param);
}

void do_repeat(vt100_console auto& t, string_view s) {
    unsigned int param = 1;

    if (!s.empty()) {
        param = 0;

        for (auto c : s) {
            if (c < '0' || c > '9')
                return;

            param *= 10;
            param += (unsigned int)(c - '0');
        }
    }

    if (param == 0)
        return;

    for (unsigned int i = 0; i < param; i++) {
        t.print_chars(span(&last_char, 1));
    }

    t.cursor_changed();
}

void delete_chars(vt100_console auto& t, string_view s) {
    unsigned int num = 1;

    if (!s.empty()) {
        num = 0;

        for (auto c : s) {
            if (c < '0' || c > '9')
                return;

            num *= 10;
            num += (unsigned int)(c - '0');
        }

        if (num == 0)
            return;
    }

    t.delete_chars(num);
}

void insert_chars(vt100_console auto& t, string_view s) {
    unsigned int num = 1;

    if (!s.empty()) {
        num = 0;

        for (auto c : s) {
            if (c < '0' || c > '9')
                return;

            num *= 10;
            num += (unsigned int)(c - '0');
        }

        if (num == 0)
            return;
    }

    t.insert_chars(num);
}

void erase_chars(vt100_console auto& t, string_view s) {
    unsigned int num = 1;

    if (!s.empty()) {
        num = 0;

        for (auto c : s) {
            if (c < '0' || c > '9')
                return;

            num *= 10;
            num += (unsigned int)(c - '0');
        }

        if (num == 0)
            return;
    }

    t.erase_chars(num);
}

void set_scroll_region(vt100_console auto& t, string_view s) {
    unsigned int top = 1, bottom = t.console_rows;

    if (!s.empty()) {
        top = 0;

        while (!s.empty()) {
            if (s.front() == ';')
                break;

            if (s.front() < '0' || s.front() > '9')
                return;

            top *= 10;
            top += (unsigned int)(s.front() - '0');

            s = string_view(s.data() + 1, s.size() - 1);
        }

        if (!s.empty()) {
            if (s.front() == ';')
                s = string_view(s.data() + 1, s.size() - 1);

            if (!s.empty()) {
                bottom = 0;

                while (!s.empty()) {
                    if (s.front() < '0' || s.front() > '9')
                        return;

                    bottom *= 10;
                    bottom += (unsigned int)(s.front() - '0');

                    s = string_view(s.data() + 1, s.size() - 1);
                }
            }
        }
    }

    top = min(max((unsigned int)1, top), t.console_rows);
    bottom = min(max((unsigned int)1, bottom), t.console_rows);

    if (bottom < top)
        bottom = top;

    t.scroll_top = top - 1;
    t.scroll_bottom = bottom - 1;

    // old position
    t.cursor_changed();

    t.cursor_x = 0;
    t.cursor_y = 0;

    // new position
    t.cursor_changed();
}

void terminal_reset(vt100_console auto& t) {
    esc_state = esc_state::normal;
    utf8_chars = 0;
    escape_buffer_len = 0;
    using_acs = false;
    last_char = 0;
    saved_cursor_x = 0;
    saved_cursor_y = 0;
    saved_using_acs = false;

    t.exit_alt_screen();

    t.decckm = false;
    t.auto_wrap = true;
    t.insert_mode = false;
    t.scroll_top = 0;
    t.scroll_bottom = t.console_rows - 1;
    t.cursor_x = 0;
    t.cursor_y = 0;

    if constexpr (vt100_console_with_tab_stops<remove_reference_t<decltype(t)>>) {
        for (unsigned int i = 0; i < t.tab_stops_count; i++) {
            t.tab_stops[i] = i > 0 && i % 8 == 0;
        }
    }

    t.handle_sgr_part_num(0);
    t.erase_in_display(2);
    t.cursor_changed();
}

void next_line(vt100_console auto& t) {
    t.cursor_changed();

    t.cursor_x = 0;

    if (t.cursor_y == t.scroll_bottom)
        t.console_overflow(1);
    else
        t.cursor_y++;

    t.cursor_changed();
}

void reverse_index(vt100_console auto& t) {
    if (t.cursor_y == t.scroll_top)
        t.scroll_down(1);
    else {
        // old position
        t.cursor_changed();

        t.cursor_y--;

        // new position
        t.cursor_changed();
    }
}

void insert_lines(vt100_console auto& t, string_view s) {
    unsigned int rows = 1;

    if (!s.empty()) {
        rows = 0;

        for (auto c : s) {
            if (c < '0' || c > '9')
                return;

            rows *= 10;
            rows += (unsigned int)(c - '0');
        }

        if (rows == 0)
            return;
    }

    if (t.cursor_y < t.scroll_top || t.cursor_y > t.scroll_bottom)
        return;

    t.cursor_changed();
    t.cursor_x = 0;
    t.insert_lines(rows);
    t.cursor_changed();
}

void delete_lines(vt100_console auto& t, string_view s) {
    unsigned int rows = 1;

    if (!s.empty()) {
        rows = 0;

        for (auto c : s) {
            if (c < '0' || c > '9')
                return;

            rows *= 10;
            rows += (unsigned int)(c - '0');
        }

        if (rows == 0)
            return;
    }

    if (t.cursor_y < t.scroll_top || t.cursor_y > t.scroll_bottom)
        return;

    t.cursor_changed();
    t.cursor_x = 0;
    t.delete_lines(rows);
    t.cursor_changed();
}

void handle_osc_sequence(vt100_console auto& t, string_view s) {
    if (s == "\x1b]11;?") { // OSC11
        char buf[50];
        span sp(buf, sizeof(buf));
        uint32_t bg_colour = t.get_bg_colour();

        sp = format(sp, "\x1b]11;rgb:{:02x}{:02x}/{:02x}{:02x}/{:02x}{:02x}\a",
                    (bg_colour >> 16) & 0xff, (bg_colour >> 16) & 0xff,
                    (bg_colour >> 8) & 0xff, (bg_colour >> 8) & 0xff,
                    bg_colour & 0xff, bg_colour & 0xff);

        t.write_input_data(string_view(buf, (size_t)(sp.data() - buf)));
    }
}

void handle_escape_sequence(vt100_console auto& t, string_view s) {
    if (s.size() >= 3 && s[0] == 0x1b && s[1] == '[') {
        s = string_view(s.data() + 2, s.size() - 2);

        if ((s.back() >= 'A' && s.back() <= 'G') || s.back() == 'd')
            move_cursor(t, s);
        else if (s.back() == 'm')
            handle_sgr(t, string_view(s.data(), s.size() - 1));
        else if (s.back() == 'J')
            erase_in_display(t, string_view(s.data(), s.size() - 1));
        else if (s.back() == 'K')
            erase_in_line(t, string_view(s.data(), s.size() - 1));
        else if (s.back() == 'S')
            scroll_up(t, string_view(s.data(), s.size() - 1));
        else if (s.back() == 'T')
            scroll_down(t, string_view(s.data(), s.size() - 1));
        else if (s.back() == 'H' || s.back() == 'f')
            set_cursor_position(t, string_view(s.data(), s.size() - 1));
        else if (s.back() == 'b')
            do_repeat(t, string_view(s.data(), s.size() - 1));
        else if (s.back() == '@')
            insert_chars(t, string_view(s.data(), s.size() - 1));
        else if (s.back() == 'P')
            delete_chars(t, string_view(s.data(), s.size() - 1));
        else if (s.back() == 'X')
            erase_chars(t, string_view(s.data(), s.size() - 1));
        else if (s.back() == 'L')
            insert_lines(t, string_view(s.data(), s.size() - 1));
        else if (s.back() == 'M')
            delete_lines(t, string_view(s.data(), s.size() - 1));
        else if (s.back() == 'Z') {
            if constexpr (vt100_console_with_tab_stops<remove_reference_t<decltype(t)>>)
                cursor_backward_tab(t, string_view(s.data(), s.size() - 1));
        } else if (s.back() == 'g') {
            if constexpr (vt100_console_with_tab_stops<remove_reference_t<decltype(t)>>)
                clear_all_tabs(t, string_view(s.data(), s.size() - 1));
        } else if (s.back() == 'r')
            set_scroll_region(t, string_view(s.data(), s.size() - 1));
        else if (s == "?25h") {
            t.enable_cursor(true);
            t.cursor_changed();
        } else if (s == "?25l") {
            t.cursor_changed();
            t.enable_cursor(false);
        } else if (s == "?1h")
            t.decckm = true;
        else if (s == "?1l")
            t.decckm = false;
        else if (s == "?7h")
            t.auto_wrap = true;
        else if (s == "?7l")
            t.auto_wrap = false;
        else if (s == "?1049h")
            t.enter_alt_screen();
        else if (s == "?1049l")
            t.exit_alt_screen();
        else if (s == "4h")
            t.insert_mode = true;
        else if (s == "4l")
            t.insert_mode = false;
        else if (s == ">c" || s == ">0c") // DA2
            t.write_input_data("\x1b[>0;0;1c");
        else if (s == "6n") { // CPR
            char buf[50];
            span sp(buf, sizeof(buf));

            sp = format(sp, "\x1b[{};{}R", t.cursor_y + 1, t.cursor_x + 1);

            t.write_input_data(string_view(buf, (size_t)(sp.data() - buf)));
        }
    }
}

export void do_print(vt100_console auto& t, string_view s) {
    while (!s.empty()) {
        switch (esc_state) {
            case esc_state::normal: {
                auto esc = s.find('\x1b');

                if (esc == string_view::npos) {
                    do_print2(t, s);
                    s = "";
                } else {
                    if (esc > 0)
                        do_print2(t, string_view(s.data(), esc));

                    s = string_view(s.data() + esc + 1, s.size() - esc - 1);
                    esc_state = esc_state::got_esc;
                }
                break;
            }

            case esc_state::got_esc:
                switch (s.front()) {
                    case '[':
                        escape_buffer[0] = '\x1b';
                        escape_buffer[1] = '[';
                        escape_buffer_len = 2;
                        esc_state = esc_state::in_csi;
                        s = string_view(s.data() + 1, s.size() - 1);
                        break;

                    case ']':
                        escape_buffer[0] = '\x1b';
                        escape_buffer[1] = ']';
                        escape_buffer_len = 2;
                        esc_state = esc_state::in_osc;
                        s = string_view(s.data() + 1, s.size() - 1);
                        break;

                    case '(':
                        esc_state = esc_state::in_designate;
                        s = string_view(s.data() + 1, s.size() - 1);
                        break;

                    case '7': // DECSC
                        saved_cursor_x = t.cursor_x;
                        saved_cursor_y = t.cursor_y;
                        // FIXME - should also be saving character attributes (bold, italic, colour, etc.)
                        saved_using_acs = using_acs;
                        s = string_view(s.data() + 1, s.size() - 1);
                        break;

                    case '8': // DECRC
                        t.cursor_x = saved_cursor_x;
                        t.cursor_y = saved_cursor_y;
                        t.cursor_changed();

                        using_acs = saved_using_acs;
                        s = string_view(s.data() + 1, s.size() - 1);
                        break;

                    case '=': // DECKPAM
                    case '>': // DECKPNM
                        // FIXME - support application keypad mode
                        esc_state = esc_state::normal;
                        s = string_view(s.data() + 1, s.size() - 1);
                        break;

                    case 'E':
                        next_line(t);
                        s = string_view(s.data() + 1, s.size() - 1);
                        break;

                    case 'M':
                        reverse_index(t);
                        s = string_view(s.data() + 1, s.size() - 1);
                        break;

                    case 'H': // horizontal tab set
                        if constexpr (vt100_console_with_tab_stops<remove_reference_t<decltype(t)>>) {
                            if (t.cursor_x < t.tab_stops_count)
                                t.tab_stops[t.cursor_x] = true;
                        }
                        s = string_view(s.data() + 1, s.size() - 1);
                        break;

                    case 'c': // RIS
                        terminal_reset(t);
                        s = string_view(s.data() + 1, s.size() - 1);
                        break;

                    default:
                        do_print2(t, string_view("\x1b", 1));
                        esc_state = esc_state::normal;
                        break;
                }
            break;

            case esc_state::in_csi: {
                size_t i = 0;

                while (i < s.size() && s[i] >= 0x30 && s[i] <= 0x3f) {
                    i++;
                }

                size_t room = sizeof(escape_buffer) - escape_buffer_len;

                memcpy(escape_buffer + escape_buffer_len, s.data(), min(i, room));

                escape_buffer_len += min(i, room);

                s = string_view(s.data() + i, s.size() - i);

                if (s.empty())
                    break;

                auto c = s.front();
                s = string_view(s.data() + 1, s.size() - 1);

                if (c >= 0x40 && c <= 0x7e && escape_buffer_len < sizeof(escape_buffer)) {
                    escape_buffer[escape_buffer_len] = c;
                    escape_buffer_len++;

                    handle_escape_sequence(t, string_view(escape_buffer, escape_buffer_len));
                }

                escape_buffer_len = 0;
                esc_state = esc_state::normal;
                break;
            }

            case esc_state::in_designate:
                switch (s.front()) {
                    case '0':
                        using_acs = true;
                        break;

                    case 'B':
                        using_acs = false;
                        break;
                }

                s = string_view(s.data() + 1, s.size() - 1);
                esc_state = esc_state::normal;
                break;

            case esc_state::in_osc: {
                size_t i = 0;

                while (i < s.size() && s[i] != '\a' && s[i] != '\x1b') {
                    i++;
                }

                size_t room = sizeof(escape_buffer) - escape_buffer_len;

                memcpy(escape_buffer + escape_buffer_len, s.data(), min(i, room));
                escape_buffer_len += min(i, room);
                s = string_view(s.data() + i, s.size() - i);

                if (s.empty())
                    break;

                if (s.front() != '\a' && (s.size() < 2 || s[1] != '\\')) {
                    esc_state = esc_state::got_esc;
                    s = string_view(s.data() + 1, s.size() - 1);
                    escape_buffer_len = 0;
                    break;
                }

                if (s.front() == '\a')
                    s = string_view(s.data() + 1, s.size() - 1);
                else
                    s = string_view(s.data() + 2, s.size() - 2);

                handle_osc_sequence(t, string_view(escape_buffer, escape_buffer_len));
                esc_state = esc_state::normal;
                escape_buffer_len = 0;

                break;
            }
        }
    }

    t.move_cursor();
}
