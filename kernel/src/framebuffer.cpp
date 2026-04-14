module;

#include <span>
#include <stdint.h>
#include <string.h>

export module framebuffer;

using namespace std;

export enum class fb_transform {
    normal,
    flipped,
    mirrored,
    rotated90,
    rotated180,
    rotated270,
};

export fb_transform fb_transform = fb_transform::normal;

extern "C" {
    export uint32_t fb_pixels_per_scan_line;
}

export span<uint32_t> framebuffer, shadow_framebuffer;
export uint32_t fb_width, fb_height, fb_dpi;
export uint32_t shadow_fb_width, shadow_fb_height, shadow_fb_pixels_per_scan_line;
export void (*flush_framebuffer)(unsigned int x, unsigned int y,
                                 unsigned int width, unsigned int height);
export void (*move_cursor)(uint32_t x, uint32_t y);
export void (*set_cursor)(uint32_t x, uint32_t y, span<const uint32_t> buf,
                          uint32_t hot_x, uint32_t hot_y);

using copy_scanline_func = void (*)(void*, const void*, size_t);

export copy_scanline_func copy_scanline = [](void* dst, const void* src, size_t n) {
    memcpy(dst, src, n);
};

using copy_scanline_mirrored_func = void (*)(uint32_t*, const uint32_t*,
                                             uint64_t, uint64_t);

export void copy_scanline_mirrored_slow(uint32_t* dst, const uint32_t* src,
                                        uint64_t x, uint64_t length);

export copy_scanline_mirrored_func copy_scanline_mirrored = copy_scanline_mirrored_slow;

export size_t copy_scanline_granularity = sizeof(uint32_t);

using flush_scanlines_rotated90_func = void (*)(uint32_t, uint32_t);
void flush_scanlines_rotated90_slow(uint32_t min_y, uint32_t max_y);
export flush_scanlines_rotated90_func flush_scanlines_rotated90 = flush_scanlines_rotated90_slow;

export void create_boot_framebuffer_section();
export void create_other_framebuffer_section();

#ifdef __x86_64__
export void flush_scanlines_rotated90_avx2(uint32_t min_y, uint32_t max_y);
export void flush_scanlines_rotated90_avx512(uint32_t min_y, uint32_t max_y);
#endif
