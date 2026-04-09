module;

#include <span>
#include <stdint.h>
#include <string.h>

export module framebuffer;

using namespace std;

export span<uint32_t> framebuffer, shadow_framebuffer;
export uint32_t fb_width, fb_height, fb_pixels_per_scan_line, fb_dpi;
export uint32_t shadow_fb_pixels_per_scan_line;
export void (*flush_framebuffer)(unsigned int x, unsigned int y,
                                 unsigned int width, unsigned int height);
export void (*move_cursor)(uint32_t x, uint32_t y);
export void (*set_cursor)(uint32_t x, uint32_t y, span<const uint32_t> buf,
                          uint32_t hot_x, uint32_t hot_y);

using copy_scanline_func = void (*)(void*, const void*, size_t);

export copy_scanline_func copy_scanline = [](void* dst, const void* src, size_t n) {
    memcpy(dst, src, n);
};

export size_t copy_scanline_granularity = sizeof(uint32_t);

export void create_boot_framebuffer_section();
export void create_other_framebuffer_section();
