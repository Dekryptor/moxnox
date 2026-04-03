module;

#include <span>
#include <stdint.h>

export module framebuffer;

using namespace std;

export span<uint32_t> framebuffer, shadow_framebuffer;
export uint32_t fb_width, fb_height, fb_pixels_per_scan_line, fb_dpi;
export void(*flush_framebuffer)(unsigned int, unsigned int, unsigned int,
                                unsigned int);
export void(*move_cursor)(uint32_t, uint32_t);

export void create_boot_framebuffer_section();
export void create_other_framebuffer_section();
