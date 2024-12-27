#include "kernel.h"
#include "spinlock.h"
#include "objects.h"
#include "handle.h"
#include "mem.h"
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

static object_ref console_pipe;
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
static spinlock cursor_lock, bsod_lock;
static char assertion_buffer[1024];

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
    write(console_pipe, s);

    if (com1_pipe_handle)
        write(com1_pipe_handle, s);
}

void print_hex(uint64_t num, unsigned int padding) {
    char s[17];
    span sp(s, sizeof(s));

    sp = sp_itoa_hex(sp, num, padding);

    printk(string_view(s, sp.data() - s));
}

bool init_console(const loader_block_framebuffer& fb) {
    if (!fb.pa) {
        text_console = span((uint16_t*)phys_address(CONSOLE_PA), COLS * ROWS);

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

        auto ret = static_cast<pipe&>(console_pipe.get()).read(s);

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

        auto file_ref = open_inode("/console.ttf", nullopt);
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

static kernel_image* get_kernel_image_from_address(uint64_t addr) {
    auto le = kernel_images.next;

    while (le != &kernel_images) {
        auto& ent = containing_record<kernel_image>(le);

        if (addr >= (uintptr_t)ent.base_address && addr < (uintptr_t)ent.base_address + ent.size)
            return &ent;

        le = le->next;
    }

    return nullptr;
}

static void print_rip(uint64_t rip) {
    char s[200];
    auto sp = span(s, sizeof(s));

    sp = sp_itoa_hex(sp, rip, 16);

    auto img = get_kernel_image_from_address(rip);

    if (img) {
        // FIXME - also get filename and line no. from DWARF

        auto [func_name, func_start] = dwarf_find_function(*img, rip);

        sp = sp_strcpy(sp, " (");
        sp = sp_strcpy(sp, img->name);

        if (!func_name.empty()) {
            sp = sp_strcpy(sp, "!");
            sp = sp_strcpy(sp, func_name);

            if (rip != func_start) {
                sp = sp_strcpy(sp, "+");
                sp = sp_itoa_hex(sp, rip - func_start);
            }
        } else {
            sp = sp_strcpy(sp, "+");
            sp = sp_itoa_hex(sp, rip - (uintptr_t)img->base_address);
        }

        sp = sp_strcpy(sp, ")");
    }

    sp = sp_strcpy(sp, "\n");

    text_console_print(string_view(s, sp.data() - s), console_colour::blue, console_colour::white);
}

static const RUNTIME_FUNCTION* find_runtime_function(const kernel_image& img,
                                                     uint64_t rip) {
    rip -= (uintptr_t)img.base_address;

    auto pdata = find_pdata(img);
    if (pdata.empty())
        return nullptr;

    span<RUNTIME_FUNCTION> rt_funcs((RUNTIME_FUNCTION*)pdata.data(),
                                    pdata.size() / sizeof(RUNTIME_FUNCTION));

    for (const auto& f : rt_funcs) {
        if (rip >= f.BeginAddress && rip < f.EndAddress)
            return &f;

        if (f.BeginAddress > rip) // stored in order
            break;
    }

    return nullptr;
}

static optional<uint64_t> read_uint64_t(uint64_t addr) {
    // FIXME - do this safely
    return *(uint64_t*)addr;
}

enum class unwind_opcode {
    UWOP_PUSH_NONVOL = 0,
    UWOP_ALLOC_LARGE = 1,
    UWOP_ALLOC_SMALL = 2,
    UWOP_SET_FPREG = 3,
    UWOP_SAVE_NONVOL = 4,
    UWOP_SAVE_NONVOL_FAR = 5,
    UWOP_SAVE_XMM128 = 8,
    UWOP_SAVE_XMM128_FAR = 9,
    UWOP_PUSH_MACHFRAME = 10,
};

enum class unwind_register {
    rax = 0,
    rcx = 1,
    rdx = 2,
    rbx = 3,
    rsp = 4,
    rbp = 5,
    rsi = 6,
    rdi = 7,
    r8 = 8,
    r9 = 9,
    r10 = 10,
    r11 = 11,
    r12 = 12,
    r13 = 13,
    r14 = 14,
    r15 = 15
};

static uint64_t get_register(const CONTEXT& context, enum unwind_register reg) {
    switch (reg) {
        case unwind_register::rax:
            return context.Rax;
        case unwind_register::rcx:
            return context.Rcx;
        case unwind_register::rdx:
            return context.Rdx;
        case unwind_register::rbx:
            return context.Rbx;
        case unwind_register::rsp:
            return context.Rsp;
        case unwind_register::rbp:
            return context.Rbp;
        case unwind_register::rsi:
            return context.Rsi;
        case unwind_register::rdi:
            return context.Rdi;
        case unwind_register::r8:
            return context.R8;
        case unwind_register::r9:
            return context.R9;
        case unwind_register::r10:
            return context.R10;
        case unwind_register::r11:
            return context.R11;
        case unwind_register::r12:
            return context.R12;
        case unwind_register::r13:
            return context.R13;
        case unwind_register::r14:
            return context.R14;
        case unwind_register::r15:
            return context.R15;
    }

    return 0;
}

static void set_register(CONTEXT& context, enum unwind_register reg, uint64_t value) {
    switch (reg) {
        case unwind_register::rax:
            context.Rax = value;
            break;
        case unwind_register::rcx:
            context.Rcx = value;
            break;
        case unwind_register::rdx:
            context.Rdx = value;
            break;
        case unwind_register::rbx:
            context.Rbx = value;
            break;
        case unwind_register::rsp:
            context.Rsp = value;
            break;
        case unwind_register::rbp:
            context.Rbp = value;
            break;
        case unwind_register::rsi:
            context.Rsi = value;
            break;
        case unwind_register::rdi:
            context.Rdi = value;
            break;
        case unwind_register::r8:
            context.R8 = value;
            break;
        case unwind_register::r9:
            context.R9 = value;
            break;
        case unwind_register::r10:
            context.R10 = value;
            break;
        case unwind_register::r11:
            context.R11 = value;
            break;
        case unwind_register::r12:
            context.R12 = value;
            break;
        case unwind_register::r13:
            context.R13 = value;
            break;
        case unwind_register::r14:
            context.R14 = value;
            break;
        case unwind_register::r15:
            context.R15 = value;
            break;
    }
}

static bool unwind_context2(CONTEXT& context, const kernel_image& img,
                            const RUNTIME_FUNCTION* prt_func,
                            bool& machine_frame) {
    uint32_t func_offset;
    uint64_t frame = context.Rsp;

    while (true) {
        const auto& rt_func = *prt_func;

        if (rt_func.UnwindInfoAddress > img.size - sizeof(UNWIND_INFO))
            return false;

        auto& ui = *(UNWIND_INFO*)((uint8_t*)img.base_address + rt_func.UnwindInfoAddress);

        if (ui.version != 1)
            return false;

        if (ui.unwind_codes_count == 0)
            return true;

        if (ui.unwind_codes_count > 0) {
            if (rt_func.UnwindInfoAddress > img.size - sizeof(UNWIND_INFO) - (ui.unwind_codes_count * sizeof(UNWIND_CODE)))
                return false;

            if (context.Rip >= (uintptr_t)img.base_address + rt_func.BeginAddress &&
                context.Rip < (uintptr_t)img.base_address + rt_func.BeginAddress + ui.prologue_bytes) {
                func_offset = (uint32_t)(context.Rip - (uintptr_t)img.base_address - rt_func.BeginAddress);
            } else
                func_offset = 0xffffffff;

            span codes((UNWIND_CODE*)((uint8_t*)img.base_address + rt_func.UnwindInfoAddress + sizeof(UNWIND_INFO)),
                       ui.unwind_codes_count);

            if (ui.frame_register != 0) {
                frame = get_register(context, (enum unwind_register)ui.frame_register);
                frame -= ui.frame_offset * 16;
            }

            for (size_t i = 0; i < codes.size(); i++) {
                const auto& c = codes[i];

                if (c.offset > func_offset)
                    continue;

                switch ((enum unwind_opcode)c.code) {
                    case unwind_opcode::UWOP_PUSH_NONVOL: { // push reg (.seh_pushreg)
                        auto val = read_uint64_t(context.Rsp);

                        if (!val.has_value())
                            return false;

                        set_register(context, (enum unwind_register)c.info, *val);
                        context.Rsp += sizeof(uint64_t);
                        break;
                    }

                    case unwind_opcode::UWOP_ALLOC_LARGE: // sub rsp, n (.seh_stackalloc)
                        if (c.info == 0) {
                            if (i == codes.size() - 1)
                                return false;

                            context.Rsp += *(uint16_t*)&codes[i + 1] * 8;
                            i++;
                        } else {
                            if (i >= codes.size() - 2)
                                return false;

                            context.Rsp += *(uint32_t*)&codes[i + 1];
                            i += 2;
                        }
                        break;

                    case unwind_opcode::UWOP_ALLOC_SMALL: // sub rsp, n (.seh_stackalloc)
                        context.Rsp += (c.info * 8) + 8;
                        break;

                    case unwind_opcode::UWOP_SET_FPREG: // lea framereg, [rsp + n] (.seh_setframe)
                        context.Rsp = frame;
                        break;

                    case unwind_opcode::UWOP_SAVE_NONVOL: // mov [rsp+n], reg (.seh_savereg)
                        if (i == codes.size() - 1)
                            return false;

                        set_register(context, (enum unwind_register)c.info,
                                    frame + *(uint16_t*)&codes[i + 1] * 8);
                        i++;
                        break;

                    case unwind_opcode::UWOP_SAVE_NONVOL_FAR: // mov [rsp+n], reg (.seh_savereg)
                        if (i >= codes.size() - 2)
                            return false;

                        set_register(context, (enum unwind_register)c.info,
                                    frame + *(uint32_t*)&codes[i + 1]);
                        i += 2;
                        break;

                    case unwind_opcode::UWOP_SAVE_XMM128: // movaps [rsp+n], xmmreg (.seh_savexmm)
                        if (i == codes.size() - 1)
                            return false;

                        i++;
                        break;

                    case unwind_opcode::UWOP_SAVE_XMM128_FAR: // movaps [rsp+n], xmmreg (.seh_savexmm)
                        if (i >= codes.size() - 2)
                            return false;

                        i += 2;
                        break;

                    case unwind_opcode::UWOP_PUSH_MACHFRAME: { // interrupt (.seh_pushframe)
                        if (c.info != 0)
                            context.Rsp += sizeof(uint64_t);

                        auto val = read_uint64_t(context.Rsp);

                        if (!val.has_value())
                            return false;

                        context.Rip = *val;

                        val = read_uint64_t(context.Rsp + (3 * sizeof(uint64_t)));

                        if (!val.has_value())
                            return false;

                        context.Rsp = *val;
                        machine_frame = true;
                        break;
                    }

                    default:
                        return false;
                }
            }
        }

        if (!(ui.flags & UNW_FLAG_CHAININFO))
            break;

        uint8_t* addr = (uint8_t*)img.base_address;
        addr += rt_func.UnwindInfoAddress;
        addr += ui.unwind_codes_count * sizeof(UNWIND_CODE);

        if (ui.unwind_codes_count & 1)
            addr += sizeof(UNWIND_CODE);

        prt_func = (RUNTIME_FUNCTION*)addr;
    }

    return true;
}

static bool unwind_context(CONTEXT& context) {
    bool machine_frame = false;

    auto img = get_kernel_image_from_address(context.Rip);
    if (!img)
        return false;

    auto rt_func = find_runtime_function(*img, context.Rip);

    // RUNTIME_FUNCTION isn't compulsory for leaf functions with no stack
    if (rt_func) {
        if (!unwind_context2(context, *img, rt_func, machine_frame))
            return false;
    }

    if (!machine_frame) {
        auto rip = read_uint64_t(context.Rsp);

        if (!rip.has_value())
            return false;

        context.Rsp += sizeof(uint64_t);
        context.Rip = *rip;
    }

    return true;
}

static void print_stacktrace(CONTEXT& context) {
    // not interested in top entry (bsod function)
    if (!unwind_context(context))
        return;

    do {
        print_rip(context.Rip);

        if (!unwind_context(context))
            return;
    } while (true);
}

void bsod(string_view msg, source_location sl) {
    __asm("cli");

    lock_guard lock(bsod_lock);

    // send IPI to other cores to stop them

    auto icr1 = (volatile uint32_t*)&local_apic[0x300];

    *icr1 = 0xc0000 | halt_interrupt;

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

    // FIXME - print context?

    CONTEXT context;

    __asm __volatile(
        "mov %0, rsp\n"
        "\tmov %1, rbp\n"
        "\tcall 0f\n"
        "\t0:\n"
        "\tpop rax\n"
        "\tmov %2, rax\n"
        : "=m" (context.Rsp), "=m" (context.Rbp), "=m" (context.Rip)
        :
        : "rax"
    );

    if (framebuffer.empty()) { // FIXME - also work with framebuffer
        text_console_print("\n", console_colour::blue, console_colour::white);
        print_stacktrace(context);
    }

    __asm("call halt_isr");
    __builtin_unreachable();
}

void _assert(const char* msg, const char* file, unsigned int line) {
    span sp(assertion_buffer, sizeof(assertion_buffer));

    sp = sp_strcpy(sp, "assertion failed at ");
    sp = sp_strcpy(sp, file);
    sp = sp_strcpy(sp, ":");
    sp = sp_itoa(sp, line);
    sp = sp_strcpy(sp, ": ");
    sp = sp_strcpy(sp, msg);

    bsod(string_view(assertion_buffer, sp.data() - assertion_buffer));
}
