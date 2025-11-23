module;

#include <stdint.h>
#include <string_view>
#include <span>

export module vt100;

using namespace std;

uint8_t utf8_buf[5];
unsigned int utf8_chars = 0;
char escape_buffer[50];
size_t escape_buffer_len = 0;

template<typename T>
void print_chars(T& t, span<const char32_t> s) {
    t.print_chars(s, false);
    t.cursor_changed();
}

template<typename T>
void do_print2(T& t, span<const char> s) {
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
        } else {
            sp[0] = (char32_t)c;
            sp = span(sp.data() + 1, sp.size() - 1);
        }
    }

    if (sp.size() < (sizeof(buf) / sizeof(buf[0])))
        print_chars(t, span(buf, (sizeof(buf) / sizeof(buf[0])) - sp.size()));
}

template<typename T>
void handle_sgr_part(string_view s) {
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

    T::handle_sgr_part_num(num);
}

template<typename T>
void handle_sgr(string_view s) {
    do {
        string_view p;

        auto sc = s.find(';');

        if (sc == string_view::npos)
            p = s;
        else
            p = string_view(s.data(), sc);

        handle_sgr_part<T>(p);

        if (sc == string_view::npos)
            break;

        s = string_view(s.data() + sc + 1, s.size() - sc - 1);
    } while (true);
}

template<typename T>
void move_cursor(T& t, string_view s) {
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

template<typename T>
void scroll_up(T& t, string_view s) {
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

    t.cursor_y += rows;
    if (t.cursor_y >= t.console_rows)
        t.cursor_y = t.console_rows - 1;
}

template<typename T>
void set_cursor_position(T& t, string_view s) {
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

template<typename T>
void scroll_down(T& t, string_view s) {
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

template<typename T>
void erase_in_line(T& t, string_view s) {
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

    switch (param) {
        case 0: { // clear from cursor to end of line
            auto orig_cursor_x = t.cursor_x;

            while (t.cursor_x < t.console_cols) {
                t.print_char(' ');
                t.cursor_x++;
            }

            t.cursor_x = orig_cursor_x;
            break;
        }

        case 1: { // clear from beginning of line to cursor
            auto orig_cursor_x = t.cursor_x;

            t.cursor_x = 0;
            while (t.cursor_x < orig_cursor_x) {
                t.print_char(' ');
                t.cursor_x++;
            }

            t.cursor_x = orig_cursor_x;
            break;
        }

        case 2: { // clear entire line
            auto orig_cursor_x = t.cursor_x;

            t.cursor_x = 0;

            while (t.cursor_x < t.console_cols) {
                t.print_char(' ');
                t.cursor_x++;
            }

            t.cursor_x = orig_cursor_x;
            break;
        }
    }
}

template<typename T>
void erase_in_display(T& t, string_view s) {
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

    auto orig_cursor_x = t.cursor_x;
    auto orig_cursor_y = t.cursor_y;

    switch (param) {
        case 0: // clear from cursor to end of screen
            while (t.cursor_y < t.console_rows) {
                while (t.cursor_x < t.console_cols) {
                    t.print_char(' ');
                    t.cursor_x++;
                }

                t.cursor_x = 0;
                t.cursor_y++;
            }
            break;

        case 1: // clear from beginning of screen to cursor
            t.cursor_x = 0;
            t.cursor_y = 0;

            while (t.cursor_y < orig_cursor_y) {
                while (t.cursor_x < t.console_cols) {
                    t.print_char(' ');
                    t.cursor_x++;
                }

                t.cursor_x = 0;
                t.cursor_y++;
            }

            while (t.cursor_x < orig_cursor_x) {
                t.print_char(' ');
                t.cursor_x++;
            }

            break;

        case 2: // clear entire screen
        case 3:
            t.cursor_y = 0;
            while (t.cursor_y < t.console_rows) {
                while (t.cursor_x < t.console_cols) {
                    t.print_char(' ');
                    t.cursor_x++;
                }

                t.cursor_x = 0;
                t.cursor_y++;
            }

            break;
    }

    t.cursor_x = orig_cursor_x;
    t.cursor_y = orig_cursor_y;
}

template<typename T>
void handle_escape_sequence(T& t, string_view s) {
    if (s.size() >= 3 && s[0] == 0x1b && s[1] == '[') {
        s = string_view(s.data() + 2, s.size() - 2);

        if ((s.back() >= 'A' && s.back() <= 'G') || s.back() == 'd')
            move_cursor(t, s);
        else if (s.back() == 'm')
            handle_sgr<T>(string_view(s.data(), s.size() - 1));
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
        else if (s == "?25h") {
            t.cursor_changed();
            t.enable_cursor(true);
        } else if (s == "?25l") {
            t.cursor_changed();
            t.enable_cursor(false);
        }
    }
}

export template<typename T>
void do_print(T& t, string_view s) {
    if (escape_buffer_len != 0) {
        if (escape_buffer_len == 1 && s[0] != '[') {
            char buf[1];

            buf[0] = 0x1b;

            do_print2(t, span(buf, 1));
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
                    do_print2(t, span(escape_buffer, escape_buffer_len));
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

                // FIXME - print literally if not ENABLE_PROCESSED_OUTPUT
                handle_escape_sequence(t, string_view(escape_buffer, escape_buffer_len));

                escape_buffer_len = 0;

                if (s.empty())
                    return;
            } else {
                do_print2(t, span(escape_buffer, escape_buffer_len));
                escape_buffer_len = 0;
            }
        }
    }

loop:
    while (!s.empty()) {
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
                        do_print2(t, span(escape_buffer, escape_buffer_len));
                        escape_buffer_len = 0;

                        do_print2(t, span(s.data(), s.size()));
                    } else {
                        memcpy(escape_buffer, s.data(), param_bytes + 2);
                        escape_buffer_len = param_bytes + 2;
                    }

                    return;
                }

                if (s[param_bytes + 2] >= 0x40 && s[param_bytes + 2] <= 0x7e) {
                    // cap mid-buffer escape sequences as sizeof(escape_buffer), for consistency
                    if (param_bytes + 3 > sizeof(escape_buffer))
                        do_print2(t, span(s.data(), param_bytes + 3));
                    else {
                        // FIXME - print literally if not ENABLE_PROCESSED_OUTPUT
                        handle_escape_sequence(t, string_view(s.data(), param_bytes + 3));
                    }

                    s = string_view(s.data() + param_bytes + 3, s.size() - param_bytes - 3);
                    continue;
                }
            }

            char buf[1];

            buf[0] = c;

            do_print2(t, span(buf, 1));
            s = string_view(s.data() + 1, s.size() - 1);
        } else {
            auto esc = s.find_first_of(0x1b);

            if (esc == string_view::npos) {
                do_print2(t, s);
                break;
            } else {
                do_print2(t, string_view(s.data(), esc));
                s = string_view(s.data() + esc, s.size() - esc);
            }
        }
    }

    t.move_cursor();
}
