module;

#include <span>
#include <stdint.h>

export module framebuffer;

import spinlock;

using namespace std;

export span<uint32_t> framebuffer, shadow_framebuffer;
export uint32_t fb_width, fb_height, fb_pixels_per_scan_line, fb_dpi;
export void(*flush_framebuffer)(unsigned int, unsigned int, unsigned int,
                                unsigned int);

#ifdef __x86_64__
export span<uint16_t> text_console;
export uint32_t text_console_cols, text_console_rows;
export spinlock text_cursor_lock;
#endif

export void create_boot_framebuffer_section();
export void create_other_framebuffer_section();

#ifdef __x86_64__
export void create_text_console_section();
#endif
