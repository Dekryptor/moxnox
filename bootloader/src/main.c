#include <stdint.h>
#include <stdbool.h>
#include "multiboot.h"

#define COLS 80
#define ROWS 25

enum map_type {
    mem_free,
    mem_reserved,
    mem_bootloader
};

typedef struct {
    uint64_t addr;
    uint64_t len;
    enum map_type type;
} map_entry;

static map_entry map[1024];
static unsigned int num_map_entries = 0;

static uint16_t* video = (uint16_t*)0xB8000;
static unsigned int cursor_x = 0, cursor_y = 0;

extern char _image_base, ebss;

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

static void print_hex(uint64_t num) {
    bool first = true;
    char s[2];

    if (num == 0) {
        print("0");
        return;
    }

    s[1] = 0;

    for (unsigned int i = 0; i < 16; i++) {
        if (!first || num & 0xf000000000000000) {
            uint64_t n = num >> 60;

            if (n <= 9)
                s[0] = n + '0';
            else
                s[0] = n - 0xa + 'a';

            print(s);
            first = false;
        }

        num <<= 4;
    }
}

static bool parse_mmap(multiboot_info_t* mb) {
    multiboot_memory_map_t* m = (multiboot_memory_map_t*)mb->mmap_addr;

    while ((uintptr_t)m < mb->mmap_addr + mb->mmap_length) {


        if (num_map_entries == sizeof(map) / sizeof(map_entry)) {
            print("Too many map entries.\n");
            return false;
        }

        map[num_map_entries].addr = m->addr;
        map[num_map_entries].len = m->len;

        switch (m->type) {
            case MULTIBOOT_MEMORY_AVAILABLE:
                map[num_map_entries].type = mem_free;
                break;

            default:
                map[num_map_entries].type = mem_reserved;
        }

        num_map_entries++;

        m = (multiboot_memory_map_t*)((uint8_t*)m + sizeof(uint32_t) + m->size);
    }

    // FIXME - sort
    // FIXME - remove overlaps

    // FIXME - round to page boundaries

    // FIXME - memory currently used by bootloader

    for (unsigned int i = 0; i < num_map_entries; i++) {
        print("Entry: ");
        print_hex(map[i].addr);
        print(", ");
        print_hex(map[i].len);
        print(", ");
        print_hex(map[i].type);
        print("\n");
    }

    return true;
}

void __attribute__((cdecl)) main(multiboot_info_t* mb, uint32_t magic) {
    __builtin_memset(video, 0, COLS * ROWS * sizeof(uint16_t));

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        print("Multiboot magic value not found.\n");
        return;
    }

    if (!(mb->flags & 0x40)) {
        print("Memory map not provided.\n");
        return;
    }

    print("Image start: ");
    print_hex((uint32_t)&_image_base);
    print("\n");
    print("Image end: ");
    print_hex((uint32_t)&ebss);
    print("\n");

    if (!parse_mmap(mb))
        return;

    while (1) {
    }
}
