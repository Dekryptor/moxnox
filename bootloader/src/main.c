#include "bootloader.h"
#include "multiboot.h"

#define COLS 80
#define ROWS 25

#define KERNEL_ADDR 0xfffff80000000000

map_entry map[256];
unsigned int num_map_entries = 0;

static multiboot_module_t mods[256];
static unsigned int num_modules = 0;

static uint16_t* video = (uint16_t*)0xB8000;
static unsigned int cursor_x = 0, cursor_y = 0;

extern char _image_base, ebss;

uint64_t entry_point;

static void putc(uint8_t x, uint8_t y,char c) {
    video[y * COLS + x] = (7 << 8) | c;
}

void* memcpy(void* dest, const void* src, size_t n) {
    while (n >= sizeof(uint32_t)) {
        *(uint32_t*)dest = *(uint32_t*)src;

        dest = (uint8_t*)dest + sizeof(uint32_t);
        src = (uint8_t*)src + sizeof(uint32_t);

        n -= sizeof(uint32_t);
    }

    while (n >= sizeof(uint16_t)) {
        *(uint16_t*)dest = *(uint16_t*)src;

        dest = (uint8_t*)dest + sizeof(uint16_t);
        src = (uint8_t*)src + sizeof(uint16_t);

        n -= sizeof(uint16_t);
    }

    while (n >= sizeof(uint8_t)) {
        *(uint8_t*)dest = *(uint8_t*)src;

        dest = (uint8_t*)dest + sizeof(uint8_t);
        src = (uint8_t*)src + sizeof(uint8_t);

        n -= sizeof(uint8_t);
    }

    return dest;
}

void* memset(void* s, int c, size_t n) {
    void* orig_s = s;

    // FIXME - faster if we make sure we're aligned (also in memcpy)?

    uint32_t v;

    v = 0;

    for (unsigned int i = 0; i < sizeof(uint32_t); i++) {
        v <<= 8;
        v |= c & 0xff;
    }

    while (n >= sizeof(uint32_t)) {
        *(uint32_t*)s = v;

        s = (uint8_t*)s + sizeof(uint32_t);
        n -= sizeof(uint32_t);
    }

    while (n > 0) {
        *(uint8_t*)s = c;

        s = (uint8_t*)s + 1;
        n--;
    }

    return orig_s;
}

static void overflow() {
    __builtin_memcpy(video, video + COLS, (ROWS - 1) * COLS * sizeof(uint16_t));
    cursor_y = ROWS - 1;
}

void print(const char* s) {
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

void print_hex(uint64_t num) {
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

static void insert_map_entry(int i) {
    for (int j = num_map_entries - 1; j >= i; j--) {
        __builtin_memcpy(&map[j+1], &map[j], sizeof(map_entry));
    }
}

static void remove_map_entry(int i) {
    for (unsigned int j = i; j < num_map_entries; j++) {
        __builtin_memcpy(&map[j], &map[j+1], sizeof(map_entry));
    }

    num_map_entries--;
}

bool add_map_entry(uint64_t start, uint64_t len, enum map_type type) {
    if (start & (PAGE_SIZE - 1)) {
        len += start & (PAGE_SIZE - 1);
        start -= start & (PAGE_SIZE - 1);
    }

    if (len & (PAGE_SIZE - 1))
        len += PAGE_SIZE - (len & (PAGE_SIZE - 1));

    for (unsigned int i = 0; i < num_map_entries; i++) {
        if (map[i].addr >= start && map[i].addr + map[i].len <= start + len) { // remove whole entry
            remove_map_entry(i);
            i--;
        } else if (map[i].addr >= start && map[i].addr < start + len) { // remove start
            map[i].len = map[i].addr + map[i].len - start - len;
            map[i].addr = start + len;

            if (map[i].len == 0) {
                remove_map_entry(i);
                i--;
            }
        } else if (map[i].addr + map[i].len > start && map[i].addr + map[i].len <= start + len) { // remove end
            map[i].len = start - map[i].addr;

            if (map[i].len == 0) {
                remove_map_entry(i);
                i--;
            }
        } else if (start > map[i].addr && start + len < map[i].addr + map[i].len) { // remove middle
            if (num_map_entries == sizeof(map) / sizeof(map_entry)) {
                print("Too many map entries.\n");
                return false;
            }

            insert_map_entry(i);
            num_map_entries++;

            map[i].len = start - map[i].addr;
            map[i+1].len = map[i+1].addr + map[i+1].len - start - len;
            map[i+1].addr = start + len;
        }
    }

    if (num_map_entries == sizeof(map) / sizeof(map_entry)) {
        print("Too many map entries.\n");
        return false;
    }

    if (start == 0) {
        insert_map_entry(0);
        map[0].addr = 0;
        map[0].len = len;
        map[0].type = type;
        num_map_entries++;

        return true;
    }

    for (unsigned int i = 0; i < num_map_entries; i++) {
        if (map[i].addr + map[i].len == start) {
            insert_map_entry(i+1);

            map[i+1].addr = start;
            map[i+1].len = len;
            map[i+1].type = type;

            num_map_entries++;
        }
    }

    return true;
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

    if (num_map_entries == 0) {
        print("No map entries found.\n");
        return false;
    }

    // FIXME - sort
    // FIXME - remove overlaps

    // add missing entries
    if (map[0].addr != 0) {
        if (num_map_entries == sizeof(map) / sizeof(map_entry)) {
            print("Too many map entries.\n");
            return false;
        }

        insert_map_entry(0);

        map[0].addr = 0;
        map[0].len = map[1].addr;
        map[0].type = mem_reserved;
    }

    for (unsigned int i = 0; i < num_map_entries - 1; i++) {
        if (map[i].addr + map[i].len < map[i+1].addr) {
            if (num_map_entries == sizeof(map) / sizeof(map_entry)) {
                print("Too many map entries.\n");
                return false;
            }

            insert_map_entry(i+1);

            map[i+1].addr = map[i].addr + map[i].len;
            map[i+1].len = map[i+2].addr - map[i+1].addr;
            map[i+1].type = mem_reserved;

            num_map_entries++;
        }
    }

    // merge runs
    for (unsigned int i = 0; i < num_map_entries - 1; i++) {
        if (map[i].addr + map[i].len == map[i+1].addr && map[i].type == map[i+1].type) {
            map[i].len = map[i+1].addr + map[i+1].len - map[i].addr;

            remove_map_entry(i+1);
            i--;
        }
    }

    // round to page boundaries
    for (unsigned int i = 0; i < num_map_entries; i++) {
        if ((map[i].addr + map[i].len) & (PAGE_SIZE - 1)) {
            if (map[i].type == mem_free) {
                if (i < num_map_entries - 1) {
                    map[i+1].addr -= (map[i].addr + map[i].len) & (PAGE_SIZE - 1);
                    map[i+1].len += (map[i].addr + map[i].len) & (PAGE_SIZE - 1);
                }

                map[i].len -= (map[i].addr + map[i].len) & (PAGE_SIZE - 1);

                if (map[i].len == 0) {
                    remove_map_entry(i);
                    i--;
                }
            } else {
                if (i < num_map_entries - 1) {
                    map[i+1].addr += PAGE_SIZE - ((map[i].addr + map[i].len) & (PAGE_SIZE - 1));
                    map[i+1].len -= PAGE_SIZE - ((map[i].addr + map[i].len) & (PAGE_SIZE - 1));

                    if (map[i+1].len == 0)
                        remove_map_entry(i+1);
                }

                map[i].len += PAGE_SIZE - ((map[i].addr + map[i].len) & (PAGE_SIZE - 1));
            }
        }
    }

    // reserve first page, so we don't have addresses of 0
    if (!add_map_entry(0, 0x1000, mem_reserved))
        return false;

    if (!add_map_entry((uintptr_t)&_image_base, (uintptr_t)&ebss - (uintptr_t)&_image_base, mem_bootloader))
        return false;

    return true;
}

void __attribute__((cdecl)) main(multiboot_info_t* mb, uint32_t magic) {
    __builtin_memset(video, 0, COLS * ROWS * sizeof(uint16_t));

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        print("Multiboot magic value not found.\n");
        return;
    }

    // FIXME - check multiboot checksum

    if (!(mb->flags & MULTIBOOT_INFO_MODS)) {
        print("Module list not provided.\n");
        return;
    }

    if (mb->mods_count == 0) {
        print("Kernel not specified.\n");
        return;
    }

    if (!(mb->flags & MULTIBOOT_INFO_MEM_MAP)) {
        print("Memory map not provided.\n");
        return;
    }

    // FIXME - do CPUID to make sure amd64

    print("Image start: ");
    print_hex((uint32_t)&_image_base);
    print("\n");
    print("Image end: ");
    print_hex((uint32_t)&ebss);
    print("\n");

    if (!parse_mmap(mb))
        return;

    num_modules = mb->mods_count;
    __builtin_memcpy(&mods, (void*)mb->mods_addr, sizeof(multiboot_module_t) * num_modules);

    mb = NULL;

    for (unsigned int i = 0; i < num_modules; i++) {
        if (!add_map_entry(mods[i].mod_start, mods[i].mod_end - mods[i].mod_start, mem_bootloader))
            return;
    }

    if (!load_pe((void*)mods[0].mod_start, mods[0].mod_end - mods[0].mod_start, KERNEL_ADDR, &entry_point))
        return;

    for (unsigned int i = 0; i < num_map_entries; i++) {
        print("Entry: ");
        print_hex(map[i].addr);
        print(", ");
        print_hex(map[i].len);
        print(", ");
        print_hex(map[i].type);
        print("\n");
    }

    print("Entry point: ");
    print_hex(entry_point);
    print("\n");

    if (!setup_paging(KERNEL_ADDR))
        return;

    go();
}
