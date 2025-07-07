module;

#include <stdint.h>
#include <string_view>

export module vt100;

using namespace std;

extern "C++" unsigned int console_rows, console_cols; // FIXME

uint8_t utf8_buf[5];
unsigned int utf8_chars = 0;
char escape_buffer[50];
size_t escape_buffer_len = 0;

template<typename T>
void print_char(T& t, char32_t c) {
    if (c == '\n') {
        t.print_char('\n');

        t.cursor_x = 0;
        t.cursor_y++;

        if (t.cursor_y == console_rows) {
            T::console_overflow();
            t.cursor_y = console_rows - 1;
        }
    } else if (c == '\b') {
        if (t.cursor_x == 0 && t.cursor_y == 0)
            return;

        t.print_char('\b');
    } else {
        if (c < 0x20)
            return;

        t.print_char(c);

        t.cursor_x++;

        if (t.cursor_x == console_cols) {
            t.cursor_x = 0;
            t.cursor_y++;

            if (t.cursor_y == console_rows) {
                T::console_overflow();
                t.cursor_y = console_rows - 1;
            }
        }
    }

    t.cursor_changed();
}

template<typename T>
void do_print2(T& t, char c) {
    if (utf8_chars != 0) {
        if (!(c & 0x80) || utf8_chars == sizeof(utf8_buf)) {
            print_char(t, 0xfffd);
            print_char(t, (char32_t)c);
            utf8_chars = 0;
            return;
        }

        utf8_buf[utf8_chars] = (uint8_t)c;
        utf8_chars++;

        if ((utf8_buf[0] & 0xe0) == 0xc0 && utf8_chars == 2) {
            if ((utf8_buf[1] & 0xc0) != 0x80) {
                print_char(t, 0xfffd);
                utf8_chars = 0;
                return;
            }

            auto c = (char32_t)(((utf8_buf[0] & 0x1f) << 6) | (utf8_buf[1] & 0x3f));

            print_char(t, c);
            utf8_chars = 0;
            return;
        } else if ((utf8_buf[0] & 0xf0) == 0xe0) {
            if (utf8_chars == 2) {
                if ((utf8_buf[1] & 0xc0) != 0x80) {
                    print_char(t, 0xfffd);
                    utf8_chars = 0;
                }
            } else {
                if ((utf8_buf[2] & 0xc0) != 0x80) {
                    print_char(t, 0xfffd);
                    utf8_chars = 0;
                    return;
                }

                auto c = (char32_t)(((utf8_buf[0] & 0xf) << 12) | ((utf8_buf[1] & 0x3f) << 6) | (utf8_buf[2] & 0x3f));

                print_char(t, c);
                utf8_chars = 0;
            }

            return;
        } else if ((utf8_buf[0] & 0xf8) == 0xf0) {
            if (utf8_chars == 2) {
                if ((utf8_buf[1] & 0xc0) != 0x80) {
                    print_char(t, 0xfffd);
                    utf8_chars = 0;
                }
            } else if (utf8_chars == 3) {
                if ((utf8_buf[2] & 0xc0) != 0x80) {
                    print_char(t, 0xfffd);
                    utf8_chars = 0;
                }
            } else {
                if ((utf8_buf[3] & 0xc0) != 0x80) {
                    print_char(t, 0xfffd);
                    utf8_chars = 0;
                    return;
                }

                auto c = (char32_t)(((utf8_buf[0] & 0x7) << 18) | ((utf8_buf[1] & 0x3f) << 12) | ((utf8_buf[2] & 0x3f) << 6) | (utf8_buf[3] & 0x3f));

                print_char(t, c);
                utf8_chars = 0;
            }

            return;
        }
    }

    if ((c & 0xc0) == 0xc0) { // first character of UTF-8 sequence
        utf8_buf[0] = (uint8_t)c;
        utf8_chars = 1;
        return;
    } else if (c & 0x80) {
        print_char(t, 0xfffd);
        return;
    }

    print_char(t, (char32_t)c);
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
        if (t.cursor_y < console_rows - param)
            t.cursor_y += param;
        else
            t.cursor_y = console_rows - 1;
    } else if (s.back() == 'C') { // right
        if (t.cursor_x < console_cols - param)
            t.cursor_x += param;
        else
            t.cursor_x = console_cols - 1;
    } else if (s.back() == 'D') { // left
        if (t.cursor_x > param - 1)
            t.cursor_x -= param;
    } else if (s.back() == 'E') { // cursor next line
        t.cursor_x = 0;

        if (t.cursor_y < console_rows - param)
            t.cursor_y += param;
        else
            t.cursor_y = console_rows - 1;
    } else if (s.back() == 'F') { // cursor previous line
        t.cursor_x = 0;

        if (t.cursor_y > param - 1)
            t.cursor_y -= param;
        else
            t.cursor_y = 0;
    } else if (s.back() == 'G') { // cursor horizontal absolute
        if (param < 1)
            param = 1;

        if (param > console_cols)
            t.cursor_x = console_cols - 1;
        else
            t.cursor_x = param - 1;
    } else if (s.back() == 'd') { // vertical position absolute
        if (param < 1)
            param = 1;

        if (param > console_rows)
            t.cursor_y = console_rows - 1;
        else
            t.cursor_y = param - 1;
    }

    // new position
    t.cursor_changed();
}

template<typename T>
void scroll_up(string_view s) {
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

    if (rows > console_rows)
        rows = console_rows;

    T::console_overflow(rows);
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

    if (x >= console_cols)
        x = console_cols;

    if (y >= console_rows)
        y = console_rows;

    t.cursor_x = x - 1;
    t.cursor_y = y - 1;

    // new position
    t.cursor_changed();
}

template<typename T>
void scroll_down(string_view s) {
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

    if (rows > console_rows)
        rows = console_rows;

    T::scroll_down(rows);
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

            while (t.cursor_x < console_cols) {
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

            while (t.cursor_x < console_cols) {
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
            while (t.cursor_y < console_rows) {
                while (t.cursor_x < console_cols) {
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
                while (t.cursor_x < console_cols) {
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
            t.cursor_x = 0;
            t.cursor_y = 0;

            while (t.cursor_y < console_rows) {
                while (t.cursor_x < console_cols) {
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
            scroll_up<T>(string_view(s.data(), s.size() - 1));
        else if (s.back() == 'T')
            scroll_down<T>(string_view(s.data(), s.size() - 1));
        else if (s.back() == 'H' || s.back() == 'f')
            set_cursor_position(t, string_view(s.data(), s.size() - 1));
        else if (s == "?25h") {
            t.cursor_changed();
            T::enable_cursor(true);
        } else if (s == "?25l") {
            t.cursor_changed();
            T::enable_cursor(false);
        }
    }
}

export template<typename T>
void do_print(T& t, string_view s) {
    if (escape_buffer_len != 0) {
        if (escape_buffer_len == 1 && s[0] != '[') {
            do_print2(t, 0x1b);
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
                        do_print2(t, escape_buffer[i]);
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

                // FIXME - print literally if not ENABLE_PROCESSED_OUTPUT
                handle_escape_sequence(t, string_view(escape_buffer, escape_buffer_len));

                escape_buffer_len = 0;

                if (s.empty())
                    return;
            } else {
                for (size_t i = 0; i < escape_buffer_len; i++) {
                    do_print2(t, escape_buffer[i]);
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
                            do_print2(t, escape_buffer[i]);
                        }
                        escape_buffer_len = 0;

                        for (size_t i = 0; i < s.size(); i++) {
                            do_print2(t, s[i]);
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
                            do_print2(t, s[i]);
                        }
                    } else {
                        // FIXME - print literally if not ENABLE_PROCESSED_OUTPUT
                        handle_escape_sequence(t, string_view(s.data(), param_bytes + 3));
                    }

                    s = string_view(s.data() + param_bytes + 2, s.size() - param_bytes - 2);
                    continue;
                }
            }
        }

        do_print2(t, c);
    }

    t.move_cursor();
}
