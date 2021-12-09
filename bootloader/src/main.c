#include <stdint.h>

enum color {
    BLACK = 0,
    BRIGHT = 7
};

#define COLS 80
#define ROWS 25

static uint16_t* video = (uint16_t*)0xB8000;
static unsigned int cursor_x = 0, cursor_y = 0;

static void putc(uint8_t x, uint8_t y,char c) {
    video[y * COLS + x] = (7 << 8) | c;
}

static void overflow() {
    __builtin_memcpy(video, video + COLS, (ROWS - 1) * COLS * sizeof(uint16_t));
    cursor_y = ROWS - 1;
}

static void print(const char* s) {
    while (*s != 0) {
        if (*s == '\n') {
            s++;
            cursor_x = 0;
            cursor_y++;

            if (cursor_y == ROWS)
                overflow();
        } else {
            putc(cursor_x, cursor_y, *s);

            s++;
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

int __attribute__((noreturn, cdecl)) main(void* multiboot) {
    __builtin_memset(video, 0, COLS * ROWS * sizeof(uint16_t));

    print("A\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\nN\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ");

    while (1) {
    }
}
