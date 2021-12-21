#include "kernel.h"
#include "spinlock.h"
#include <span>
#include <cstring>
#include <bit>

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

static span<uint16_t> video;
static uint8_t cursor_x = 0, cursor_y = 0;
static spinlock console_lock;

static void early_print(string_view s);
decltype(&safe_print) print = early_print;

size_t strlen(const char* s) {
    size_t i = 0;

    while (s[i] != 0) {
        i++;
    }

    return i;
}

static void putc(uint8_t x, uint8_t y, char c) {
    video[(y * COLS) + x] = (uint16_t)(((uint16_t)bg_colour << 12) | ((uint16_t)fg_colour << 8) | c);
}

static void overflow() {
    memcpy(video.data(), &video[COLS], (ROWS - 1) * COLS * sizeof(uint16_t));
    memset(video.data() + ((ROWS - 1) * COLS), 0, COLS * sizeof(uint16_t));
    cursor_y = ROWS - 1;
}

static void early_print(string_view s) {
    for (auto c : s) {
        if (c == '\n') {
            cursor_x = 0;
            cursor_y++;

            if (cursor_y == ROWS)
                overflow();
        } else {
            putc(cursor_x, cursor_y, c);

            cursor_x++;

            if (cursor_x == COLS) {
                cursor_x = 0;
                cursor_y++;

                if (cursor_y == ROWS)
                    overflow();
            }
        }
    }
}

void safe_print(string_view s) {
    lock_guard lock(console_lock);

    early_print(s);
}

void print_hex(uint64_t num, unsigned int padding) {
    bool first = true;
    char s[2];

    if (num == 0 && padding == 0) {
        print("0");
        return;
    }

    s[1] = 0;

    for (unsigned int i = 0; i < 16; i++) {
        if (!first || padding == 16 - i || num & 0xf000000000000000) {
            uint64_t n = num >> 60;

            if (n <= 9)
                s[0] = (char)(n + '0');
            else
                s[0] = (char)(n - 0xa + 'a');

            print(s);
            first = false;
        }

        num <<= 4;
    }
}

bool init_console() {
    auto va = allocate_va(1);
    if (!va) {
        // FIXME - exception
        return false;
    }

    video = span((uint16_t*)va, COLS * ROWS);

    if (!map_page(CONSOLE_PA, (uintptr_t)video.data(), mapping_type::read_write))
        return false;

    // clear console
    memset(video.data(), 0, video.size_bytes());

    print("Hello\nworld\n");

    return true;
}
