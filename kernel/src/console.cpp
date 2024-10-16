#include "kernel.h"
#include "spinlock.h"
#include "unique_handle.h"
#include "objects.h"
#include <span>
#include <cstring>
#include <bit>
#include <ft2build.h>
#include <freetype/freetype.h>

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

static const uint64_t CONSOLE_PA = 0xb8000;

static span<uint16_t> text_console;
static unsigned int cursor_x = 0, cursor_y = 0;

static void early_print(string_view s);
decltype(&safe_print) _printk = early_print;

static unique_handle console_pipe;
static const unsigned int console_buffer_pages = 4;

static span<uint32_t> framebuffer, shadow_framebuffer;
static uint32_t fb_width, fb_height, fb_pixels_per_scan_line;
static FT_Library ft = nullptr;
static FT_Face face = nullptr;
static void* font_data;
static unsigned int font_width, font_height;
static unsigned int fb_console_rows, fb_console_cols;
static bool fb_console_started = false;
static uint8_t utf8_buf[5];
static unsigned int utf8_chars = 0;
static spinlock cursor_lock;

struct console_info {
    enum console_colour bg;
    enum console_colour fg;
    char32_t c;
};

static console_info* console;

size_t strlen(const char* s) {
    size_t i = 0;

    while (s[i] != 0) {
        i++;
    }

    return i;
}

static void text_console_putc(uint8_t x, uint8_t y, uint8_t c, enum console_colour bg,
                              enum console_colour fg) {
    text_console[(y * COLS) + x] = (uint16_t)(((uint16_t)bg << 12) | ((uint16_t)fg << 8) | c);
}

static void text_console_overflow() {
    memcpy(text_console.data(), &text_console[COLS], (ROWS - 1) * COLS * sizeof(uint16_t));

    for (uint8_t x = 0; x < COLS; x++) {
        text_console_putc(x, ROWS - 1, ' ', console_colour::black, console_colour::light_grey);
    }

    cursor_y = ROWS - 1;
}

__attribute((naked))
static void outb(uint16_t port, uint8_t val) {
    __asm __volatile(
        "mov al, %1\n"
        "\tmov dx, %0\n"
        "\tout dx, al\n"
        "\tret\n"
        :
        : "r" (port), "r" (val)
        : "al", "dx"
    );
}

static void text_console_move_cursor(uint8_t x, uint8_t y) {
    auto pos = (uint16_t)((y * COLS) + x);

    {
        lock_guard lock(cursor_lock);

        outb(0x3d4, 0xf);
        outb(0x3d5, (uint8_t)(pos & 0xff));
        outb(0x3d4, 0xe);
        outb(0x3d5, (uint8_t)((pos >> 8) & 0xff));
    }
}

static void text_console_print(string_view s, enum console_colour bg = console_colour::black,
                               enum console_colour fg = console_colour::light_grey) {
    for (auto c : s) {
        if (c == '\n') {
            cursor_x = 0;
            cursor_y++;

            if (cursor_y == ROWS)
                text_console_overflow();
        } else if (c == '\b') {
            if (cursor_x > 0) // FIXME
                cursor_x--;
        } else {
            text_console_putc((uint8_t)cursor_x, (uint8_t)cursor_y, c, bg, fg);

            cursor_x++;

            if (cursor_x == COLS) {
                cursor_x = 0;
                cursor_y++;

                if (cursor_y == ROWS)
                    text_console_overflow();
            }
        }
    }

    text_console_move_cursor((uint8_t)cursor_x, (uint8_t)cursor_y);
}

static void text_console_print(integral auto i, enum console_colour bg = console_colour::black,
                               enum console_colour fg = console_colour::light_grey) {
    char s[21];

    auto [ptr, ec] = to_chars(s, s + sizeof(s), i);
    text_console_print(string_view(s, ptr - s), bg, fg);
}

static void blit(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    auto src = shadow_framebuffer.data() + (y * fb_pixels_per_scan_line) + x;
    auto dst = framebuffer.data() + (y * fb_pixels_per_scan_line) + x;

    while (h > 0) {
        memcpy(dst, src, w * sizeof(uint32_t));

        src += fb_pixels_per_scan_line;
        dst += fb_pixels_per_scan_line;
        h--;
    }
}

static void draw_ft_char(char32_t cp, unsigned int fb_x, unsigned int fb_y) {
    FT_Error error;

    // FIXME - colour

    if (fb_y >= fb_height)
        return;

    error = FT_Load_Char(face, cp, FT_LOAD_RENDER | FT_RENDER_MODE_MONO);
    if (error)
        return; // FIXME - if not found, print Unicode replacement character?

    auto& bitmap = face->glyph->bitmap;
    auto buf = bitmap.buffer;

    auto width = bitmap.width;
    if (fb_x + face->glyph->bitmap_left + width > fb_width)
        width = fb_width - fb_x - face->glyph->bitmap_left;

    int y = fb_y - face->glyph->bitmap_top;
    int bottom = y + bitmap.rows;

    if (y < 0) {
        buf += bitmap.width * (face->glyph->bitmap_top - fb_y);
        y = 0;
    }

    if (bottom > (int)fb_height)
        bottom = fb_height;

    auto orig_y = y;
    auto base = shadow_framebuffer.data() + (y * fb_pixels_per_scan_line) + fb_x + face->glyph->bitmap_left;

    while (y < bottom) {
        for (unsigned int x = 0; x < width; x++) {
            if (*buf != 0) {
                // FIXME - alpha blending
                base[x] = (*buf << 16) | (*buf << 8) | *buf;
            }

            buf++;
        }

        buf += bitmap.width - width;

        base += fb_pixels_per_scan_line;
        y++;
    }

    blit(fb_x + face->glyph->bitmap_left, orig_y, width, bottom - orig_y);
}

static void fb_console_overflow() {
    auto src = shadow_framebuffer.data() + (fb_pixels_per_scan_line * font_height);
    auto dest = shadow_framebuffer.data();

    memcpy(dest, src, fb_pixels_per_scan_line * (fb_height - font_height) * sizeof(uint32_t));
    dest += fb_pixels_per_scan_line * (fb_height - font_height);

    memset(dest, 0, fb_pixels_per_scan_line * font_height * sizeof(uint32_t)); // black

    memcpy_aligned(framebuffer.data(), shadow_framebuffer.data(), framebuffer.size_bytes());

    memcpy(console, &console[fb_console_cols], (fb_console_rows - 1) * fb_console_cols * sizeof(console_info));

    auto c = &console[(fb_console_rows - 1) * fb_console_cols];

    for (unsigned int i = 0; i < fb_console_cols; i++) {
        // FIXME - set fg and bg to current colour
        c->c = 0;
        c++;
    }
}

static void rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, uint32_t col) {
    auto dest = shadow_framebuffer.data() + (fb_pixels_per_scan_line * y) + x;

    for (unsigned int i = 0; i < h; i++) {
        for (unsigned int j = 0; j < w; j++) {
            *dest = col;
            dest++;
        }

        dest += fb_pixels_per_scan_line - w;
    }

    blit(x, y, w, h);
}

static void print_char(char32_t c) {
    if (c == '\n') {
        if (console) {
            for (unsigned int i = cursor_x; i < fb_console_cols; i++) {
                console[(cursor_y * fb_console_cols) + i].c = 0;
            }
        }

        cursor_x = 0;
        cursor_y++;

        if (cursor_y == fb_console_rows) {
            fb_console_overflow();
            cursor_y = fb_console_rows - 1;
        }
    } else if (c == '\b') {
        if (cursor_x == 0 && cursor_y == 0)
            return;

        if (cursor_x > 0)
            cursor_x--;
        else {
            cursor_x = fb_console_cols - 1;
            cursor_y--;

            if (console) {
                while (cursor_x > 0 && console[(cursor_y * fb_console_cols) + cursor_x].c == 0) {
                    cursor_x--;
                }
            }

            if (cursor_x != fb_console_cols - 1)
                cursor_x++;
        }

        if (console)
            console[(cursor_y * fb_console_cols) + cursor_x].c = 0;

        rect(cursor_x * font_width, cursor_y * font_height, font_width, font_height, 0x000000);
    } else {
        draw_ft_char(c, cursor_x * font_width, (cursor_y + 1) * font_height);

        if (console)
            console[(cursor_y * fb_console_cols) + cursor_x].c = c;

        cursor_x++;

        if (cursor_x == fb_console_cols) {
            cursor_x = 0;
            cursor_y++;

            if (cursor_y == fb_console_rows) {
                fb_console_overflow();
                cursor_y = fb_console_rows - 1;
            }
        }
    }
}

static void early_print(string_view s) {
    if (framebuffer.empty()) {
        text_console_print(s);
        return;
    }

    if (!fb_console_started)
        return;

    for (auto c : s) {
        if (utf8_chars != 0) {
            if (!(c & 0x80) || utf8_chars == sizeof(utf8_buf)) {
                print_char(0xfffd);
                print_char(c);
                utf8_chars = 0;
                continue;
            }

            utf8_buf[utf8_chars] = c;
            utf8_chars++;

            if ((utf8_buf[0] & 0xe0) == 0xc0 && utf8_chars == 2) {
                if ((utf8_buf[1] & 0xc0) != 0x80) {
                    print_char(0xfffd);
                    utf8_chars = 0;
                    continue;
                }

                char32_t c = ((utf8_buf[0] & 0x1f) << 6) | (utf8_buf[1] & 0x3f);

                print_char(c);
                utf8_chars = 0;
                continue;
            } else if ((utf8_buf[0] & 0xf0) == 0xe0) {
                if (utf8_chars == 2) {
                    if ((utf8_buf[1] & 0xc0) != 0x80) {
                        print_char(0xfffd);
                        utf8_chars = 0;
                    }
                } else {
                    if ((utf8_buf[2] & 0xc0) != 0x80) {
                        print_char(0xfffd);
                        utf8_chars = 0;
                        continue;
                    }

                    char32_t c = ((utf8_buf[0] & 0xf) << 12) | ((utf8_buf[1] & 0x3f) << 6) | (utf8_buf[2] & 0x3f);

                    print_char(c);
                    utf8_chars = 0;
                }

                continue;
            } else if ((utf8_buf[0] & 0xf8) == 0xf0) {
                if (utf8_chars == 2) {
                    if ((utf8_buf[1] & 0xc0) != 0x80) {
                        print_char(0xfffd);
                        utf8_chars = 0;
                    }
                } else if (utf8_chars == 3) {
                    if ((utf8_buf[2] & 0xc0) != 0x80) {
                        print_char(0xfffd);
                        utf8_chars = 0;
                    }
                } else {
                    if ((utf8_buf[3] & 0xc0) != 0x80) {
                        print_char(0xfffd);
                        utf8_chars = 0;
                        continue;
                    }

                    char32_t c = ((utf8_buf[0] & 0x7) << 18) | ((utf8_buf[1] & 0x3f) << 12) | ((utf8_buf[2] & 0x3f) << 6) | (utf8_buf[3] & 0x3f);

                    print_char(c);
                    utf8_chars = 0;
                }

                continue;
            }
        }

        if ((c & 0xc0) == 0xc0) { // first character of UTF-8 sequence
            utf8_buf[0] = c;
            utf8_chars = 1;
            continue;
        } else if (c & 0x80) {
            print_char(0xfffd);
            continue;
        }

        print_char(c);
    }
}

void safe_print(string_view s) {
    write(console_pipe.get(), s);

    if (com1_pipe_handle)
        write(com1_pipe_handle.get(), s);
}

void print_hex(uint64_t num, unsigned int padding) {
    char s[17];
    span sp(s, sizeof(s));

    sp = sp_itoa_hex(sp, num, padding);

    printk(string_view(s, sp.data() - s));
}

bool init_console(const loader_block_framebuffer& fb) {
    if (!fb.pa) {
        auto va = allocate_va(1);
        if (!va) {
            // FIXME - exception
            printk("allocate_va failed\n");
            return false;
        }

        text_console = span((uint16_t*)va, COLS * ROWS);

        if (!map_page(CONSOLE_PA, (uintptr_t)text_console.data(), mapping_type::read_write))
            return false;

        // clear console (foreground colour needs to be initialized for cursor to show up)
        for (uint8_t y = 0; y < ROWS; y++) {
            for (uint8_t x = 0; x < COLS; x++) {
                text_console_putc(x, y, ' ', console_colour::black, console_colour::light_grey);
            }
        }
    } else {
        uint32_t size = fb.pixels_per_scan_line * fb.height * sizeof(uint32_t);

        if (size == 0) {
            printk("Framebuffer size was 0.\n");
            return false;
        }

        auto pages = bytes_to_pages(size);
        auto va = allocate_va(pages);
        if (!va) {
            printk("allocate_va failed\n");
            return false;
        }

        framebuffer = span((uint32_t*)va, fb.pixels_per_scan_line * fb.height);

        for (unsigned int i = 0; i < pages; i++) {
            if (!map_page((uintptr_t)fb.pa + (i << PAGE_SHIFT), va + (i << PAGE_SHIFT), mapping_type::write_combine)) {
                printk("map_page failed\n");
                return false;
            }
        }

        va = allocate_va(pages);
        if (!va) {
            printk("allocate_va failed\n");
            return false;
        }

        shadow_framebuffer = span((uint32_t*)va, fb.pixels_per_scan_line * fb.height);

        for (unsigned int i = 0; i < pages; i++) {
            auto pa = allocate_page();
            if (!pa) {
                printk("allocate_page failed\n");
                return false;
            }

            if (!map_page(pa, va + (i << PAGE_SHIFT), mapping_type::read_write)) {
                printk("map_page failed\n");
                return false;
            }
        }

        fb_width = fb.width;
        fb_height = fb.height;
        fb_pixels_per_scan_line = fb.pixels_per_scan_line;

        memset(framebuffer.data(), 0, framebuffer.size_bytes());
        memset(shadow_framebuffer.data(), 0, shadow_framebuffer.size_bytes());
    }

    return true;
}

static void console_event_recv() {
    do {
        uint8_t s[255];
        wait_for_object(console_pipe.get());

        auto ret = read(console_pipe.get(), s);

        // FIXME - watch out for partial UTF-8 characters, partial escape sequences, etc.

        if (ret != 0)
            early_print(string_view((char*)s, ret));
    } while (true);
}

bool init_console_pipe() {
    console_pipe = create_pipe(console_buffer_pages);
    if (!console_pipe) {
        printk("create_pipe failed\n");
        return false;
    }

    if (!name_object(console_pipe.get(), "tty")) {
        printk("name_object failed\n");
        return false;
    }

    if (framebuffer.empty()) { // gets started later if we have framebuffer
        if (!add_kernel_thread(console_event_recv)) {
            printk("add_kernel_thread failed\n");
            return false;
        }
    }

    return true;
}

static void* ft_alloc(FT_Memory, long size) {
    return malloc(size);
}

static void* ft_realloc(FT_Memory, long cur_size, long new_size, void* block) {
    void* ret;

    ret = malloc(new_size);
    if (!ret)
        return nullptr;

    memcpy(ret, block, cur_size < new_size ? new_size : cur_size);

    free(block);

    return ret;
}

static void ft_free(FT_Memory, void* block) {
    free(block);
}

extern "C"
FT_Memory FT_New_Memory() {
    auto ptr = (FT_MemoryRec_*)malloc(sizeof(FT_MemoryRec_));
    if (!ptr)
        return nullptr;

    auto& memory = *ptr;

    memory.user = nullptr;
    memory.alloc = ft_alloc;
    memory.realloc = ft_realloc;
    memory.free = ft_free;

    return &memory;
}

extern "C"
void FT_Done_Memory(FT_Memory memory) {
    free(memory);
}

bool init_fb_console() {
    FT_Error error;

    error = FT_Init_FreeType(&ft);
    if (error) {
        printk("FT_Init_FreeType failed (");
        print_hex(error);
        printk(")\n");

        return false;
    }

    uint64_t file_size;

    {
        // load console.ttf to memory

        auto file_ref = open_inode2("/console.ttf");
        if (!file_ref) {
            printk("open_inode failed\n");
            return false;
        }

        if (file_ref.get().type != object_type::inode) {
            printk("open_inode returned something other than inode\n");
            return false;
        }

        auto& inode = static_cast<class inode&>(file_ref.get());

        file_size = inode.file_size;

        if (file_size == 0) {
            printk("Font file was empty\n");
            return false;
        }

        font_data = malloc(file_size);
        if (!font_data) {
            printk("malloc failed\n");
            return false;
        }

        if (inode.read(span((uint8_t*)font_data, file_size), 0) < file_size) {
            free(font_data);
            printk("read failed\n");
            return false;
        }
    }

    error = FT_New_Memory_Face(ft, (uint8_t*)font_data, (FT_Long)file_size, 0, &face);
    if (error) {
        free(font_data);

        printk("FT_New_Memory_Face failed (");
        print_hex(error);
        printk(")\n");

        return false;
    }

    // FIXME - font size
    error = FT_Set_Char_Size(face, 24 * 64, 0, 72, 0); // FIXME - get DPI from EDID
    if (error) {
        printk("FT_Set_Char_Size failed (");
        print_hex(error);
        printk(")\n");

        return false;
    }

    error = FT_Load_Char(face, 'M', FT_LOAD_RENDER | FT_RENDER_MODE_MONO);
    if (error) {
        printk("FT_Set_Char_Size failed (");
        print_hex(error);
        printk(")\n");

        return false;
    }

    font_width = face->glyph->bitmap.width;
    font_height = face->size->metrics.height / 64;

    fb_console_cols = fb_width / font_width;
    fb_console_rows = fb_height / font_height;

    cursor_x = cursor_y = 0;

    console = (console_info*)malloc(fb_console_cols * fb_console_rows * sizeof(console_info));
    if (!console) {
        printk("out of memory\n");
        return false;
    }

    for (unsigned int i = 0; i < fb_console_cols * fb_console_rows; i++) {
        console[i].bg = console_colour::black;
        console[i].fg = console_colour::light_grey;
        console[i].c = 0;
    }

    if (!add_kernel_thread(console_event_recv)) {
        printk("add_kernel_thread failed\n");
        return false;
    }

    fb_console_started = true;

    return true;
}

static void fill_text_console(enum console_colour bg, enum console_colour fg) {
    for (auto& c : text_console) {
        c = (uint16_t)(((uint16_t)bg << 12) | ((uint16_t)fg << 8) | ' ');
    }
}

static void fill_framebuffer(uint32_t colour) {
    auto buf = shadow_framebuffer.data();

    for (unsigned int y = 0; y < fb_height; y++) {
        for (unsigned int x = 0; x < fb_width; x++) {
            *buf = colour;
            buf++;
        }

        buf += fb_pixels_per_scan_line - fb_width;
    }
}

static void disable_cursor() {
    outb(0x3d4, 0xa);
    outb(0x3d5, 0x20);
}

void bsod(string_view msg, source_location sl) {
    __asm("cli");

    // stop other cores

    auto this_cpu = current_cpu_id();

    auto icr1 = (volatile uint32_t*)&local_apic[0x300];
    auto icr2 = (volatile uint32_t*)&local_apic[0x310];

    for (unsigned int i = 0; i < num_cpus; i++) {
        if (apic_ids[i] == this_cpu)
            continue;

        *icr2 = apic_ids[i] << 24;
        *icr1 = 0x5000 | halt_interrupt;

        while (*icr1 & 0x1000) { }
    }

    cursor_x = 0;
    cursor_y = 0;

    // clear screen to blue and print message

    if (framebuffer.empty()) {
        disable_cursor();
        fill_text_console(console_colour::blue, console_colour::white);

        text_console_print(msg, console_colour::blue, console_colour::white);
        text_console_print("\n", console_colour::blue, console_colour::white);
        text_console_print(sl.file_name(), console_colour::blue, console_colour::white);
        text_console_print(":", console_colour::blue, console_colour::white);
        text_console_print(sl.line(), console_colour::blue, console_colour::white);
        text_console_print(", ", console_colour::blue, console_colour::white);
        text_console_print(sl.function_name(), console_colour::blue, console_colour::white);
        text_console_print("\n", console_colour::blue, console_colour::white);
    } else {
        fill_framebuffer(0x000080);

        // FIXME - set colour to white
        // FIXME - also print source_location info to framebuffer

        for (auto c : msg) {
            print_char(c); // FIXME - UTF-8 etc.
        }

        memcpy(framebuffer.data(), shadow_framebuffer.data(), framebuffer.size_bytes());
    }

    // FIXME - print context
    // FIXME - print stacktrace (go through DWARF sections?)

    __asm("jmp halt_isr");
}
