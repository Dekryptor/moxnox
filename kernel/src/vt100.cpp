module;

#include <stdint.h>
#include <string_view>

export module vt100;

using namespace std;

export unsigned int cursor_x = 0, cursor_y = 0; // FIXME?
extern "C++" unsigned int console_rows, console_cols; // FIXME

uint8_t utf8_buf[5];
unsigned int utf8_chars = 0;

template<typename T>
void print_char(char32_t c) {
    if (c == '\n') {
        T::print_char('\n');

        cursor_x = 0;
        cursor_y++;

        if (cursor_y == console_rows) {
            T::console_overflow();
            cursor_y = console_rows - 1;
        }
    } else if (c == '\b') {
        if (cursor_x == 0 && cursor_y == 0)
            return;

        T::print_char('\b');
    } else {
        if (c < 0x20)
            return;

        T::print_char(c);

        cursor_x++;

        if (cursor_x == console_cols) {
            cursor_x = 0;
            cursor_y++;

            if (cursor_y == console_rows) {
                T::console_overflow();
                cursor_y = console_rows - 1;
            }
        }
    }

    T::cursor_changed();
}

export template<typename T>
void do_print2(char c) {
    if (utf8_chars != 0) {
        if (!(c & 0x80) || utf8_chars == sizeof(utf8_buf)) {
            print_char<T>(0xfffd);
            print_char<T>((char32_t)c);
            utf8_chars = 0;
            return;
        }

        utf8_buf[utf8_chars] = (uint8_t)c;
        utf8_chars++;

        if ((utf8_buf[0] & 0xe0) == 0xc0 && utf8_chars == 2) {
            if ((utf8_buf[1] & 0xc0) != 0x80) {
                print_char<T>(0xfffd);
                utf8_chars = 0;
                return;
            }

            auto c = (char32_t)(((utf8_buf[0] & 0x1f) << 6) | (utf8_buf[1] & 0x3f));

            print_char<T>(c);
            utf8_chars = 0;
            return;
        } else if ((utf8_buf[0] & 0xf0) == 0xe0) {
            if (utf8_chars == 2) {
                if ((utf8_buf[1] & 0xc0) != 0x80) {
                    print_char<T>(0xfffd);
                    utf8_chars = 0;
                }
            } else {
                if ((utf8_buf[2] & 0xc0) != 0x80) {
                    print_char<T>(0xfffd);
                    utf8_chars = 0;
                    return;
                }

                auto c = (char32_t)(((utf8_buf[0] & 0xf) << 12) | ((utf8_buf[1] & 0x3f) << 6) | (utf8_buf[2] & 0x3f));

                print_char<T>(c);
                utf8_chars = 0;
            }

            return;
        } else if ((utf8_buf[0] & 0xf8) == 0xf0) {
            if (utf8_chars == 2) {
                if ((utf8_buf[1] & 0xc0) != 0x80) {
                    print_char<T>(0xfffd);
                    utf8_chars = 0;
                }
            } else if (utf8_chars == 3) {
                if ((utf8_buf[2] & 0xc0) != 0x80) {
                    print_char<T>(0xfffd);
                    utf8_chars = 0;
                }
            } else {
                if ((utf8_buf[3] & 0xc0) != 0x80) {
                    print_char<T>(0xfffd);
                    utf8_chars = 0;
                    return;
                }

                auto c = (char32_t)(((utf8_buf[0] & 0x7) << 18) | ((utf8_buf[1] & 0x3f) << 12) | ((utf8_buf[2] & 0x3f) << 6) | (utf8_buf[3] & 0x3f));

                print_char<T>(c);
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
        print_char<T>(0xfffd);
        return;
    }

    print_char<T>((char32_t)c);
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
void move_cursor(string_view s) {
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
    T::cursor_changed();

    if (s.back() == 'A') { // up
        if (cursor_y > param - 1)
            cursor_y -= param;
        else
            cursor_y = 0;
    } else if (s.back() == 'B') { // down
        if (cursor_y < console_rows - param)
            cursor_y += param;
        else
            cursor_y = console_rows - 1;
    } else if (s.back() == 'C') { // right
        if (cursor_x < console_cols - param)
            cursor_x += param;
        else
            cursor_x = console_cols - 1;
    } else if (s.back() == 'D') { // left
        if (cursor_x > param - 1)
            cursor_x -= param;
    } else if (s.back() == 'E') { // cursor next line
        cursor_x = 0;

        if (cursor_y < console_rows - param)
            cursor_y += param;
        else
            cursor_y = console_rows - 1;
    } else if (s.back() == 'F') { // cursor previous line
        cursor_x = 0;

        if (cursor_y > param - 1)
            cursor_y -= param;
        else
            cursor_y = 0;
    } else if (s.back() == 'G') { // cursor horizontal absolute
        if (param < 1)
            param = 1;

        if (param > console_cols)
            cursor_x = console_cols - 1;
        else
            cursor_x = param - 1;
    } else if (s.back() == 'd') { // vertical position absolute
        if (param < 1)
            param = 1;

        if (param > console_rows)
            cursor_y = console_rows - 1;
        else
            cursor_y = param - 1;
    }

    // new position
    T::cursor_changed();
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
void set_cursor_position(string_view s) {
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
    T::cursor_changed();

    if (x == 0)
        x = 1;

    if (y == 0)
        y = 1;

    if (x >= console_cols)
        x = console_cols;

    if (y >= console_rows)
        y = console_rows;

    cursor_x = x - 1;
    cursor_y = y - 1;

    // new position
    T::cursor_changed();
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
void erase_in_line(string_view s) {
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
            auto orig_cursor_x = cursor_x;

            while (cursor_x < console_cols) {
                T::print_char(' ');
                cursor_x++;
            }

            cursor_x = orig_cursor_x;
            break;
        }

        case 1: { // clear from beginning of line to cursor
            auto orig_cursor_x = cursor_x;

            cursor_x = 0;
            while (cursor_x < orig_cursor_x) {
                T::print_char(' ');
                cursor_x++;
            }

            cursor_x = orig_cursor_x;
            break;
        }

        case 2: { // clear entire line
            auto orig_cursor_x = cursor_x;

            cursor_x = 0;

            while (cursor_x < console_cols) {
                T::print_char(' ');
                cursor_x++;
            }

            cursor_x = orig_cursor_x;
            break;
        }
    }
}

template<typename T>
void erase_in_display(string_view s) {
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

    auto orig_cursor_x = cursor_x;
    auto orig_cursor_y = cursor_y;

    switch (param) {
        case 0: // clear from cursor to end of screen
            while (cursor_y < console_rows) {
                while (cursor_x < console_cols) {
                    T::print_char(' ');
                    cursor_x++;
                }

                cursor_x = 0;
                cursor_y++;
            }
            break;

        case 1: // clear from beginning of screen to cursor
            cursor_x = 0;
            cursor_y = 0;

            while (cursor_y < orig_cursor_y) {
                while (cursor_x < console_cols) {
                    T::print_char(' ');
                    cursor_x++;
                }

                cursor_x = 0;
                cursor_y++;
            }

            while (cursor_x < orig_cursor_x) {
                T::print_char(' ');
                cursor_x++;
            }

            break;

        case 2: // clear entire screen
        case 3:
            cursor_x = 0;
            cursor_y = 0;

            while (cursor_y < console_rows) {
                while (cursor_x < console_cols) {
                    T::print_char(' ');
                    cursor_x++;
                }

                cursor_x = 0;
                cursor_y++;
            }
            break;
    }

    cursor_x = orig_cursor_x;
    cursor_y = orig_cursor_y;
}

export template<typename T>
void handle_escape_sequence(string_view s) {
    if (s.size() >= 3 && s[0] == 0x1b && s[1] == '[') {
        s = string_view(s.data() + 2, s.size() - 2);

        if ((s.back() >= 'A' && s.back() <= 'G') || s.back() == 'd')
            move_cursor<T>(s);
        else if (s.back() == 'm')
            handle_sgr<T>(string_view(s.data(), s.size() - 1));
        else if (s.back() == 'J')
            erase_in_display<T>(string_view(s.data(), s.size() - 1));
        else if (s.back() == 'K')
            erase_in_line<T>(string_view(s.data(), s.size() - 1));
        else if (s.back() == 'S')
            scroll_up<T>(string_view(s.data(), s.size() - 1));
        else if (s.back() == 'T')
            scroll_down<T>(string_view(s.data(), s.size() - 1));
        else if (s.back() == 'H' || s.back() == 'f')
            set_cursor_position<T>(string_view(s.data(), s.size() - 1));
        else if (s == "?25h") {
            T::cursor_changed();
            T::enable_cursor(true);
        } else if (s == "?25l") {
            T::cursor_changed();
            T::enable_cursor(false);
        }
    }
}
