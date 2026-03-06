#include <pixman.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main() {
    uint16_t width = 100;
    uint16_t height = 100;

    // 1. Create a destination image (100x100, ARGB 32-bit)
    // We'll fill it with solid blue (0xFF0000FF)
    auto dest = pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height, nullptr, 0);
    pixman_color_t blue = { 0x0000, 0x0000, 0xffff, 0xffff }; // RGBA 16-bit per channel
    pixman_rectangle16_t dest_rect = { 0, 0, width, height };
    pixman_image_fill_rectangles(PIXMAN_OP_SRC, dest, &blue, 1, &dest_rect);

    // 2. Create a source image (50x50 red square)
    pixman_image_t *src = pixman_image_create_bits(PIXMAN_a8r8g8b8, 50, 50,
                                                   nullptr, 0);
    pixman_color_t red = { 0xffff, 0x0000, 0x0000, 0xffff };
    pixman_rectangle16_t src_rect = { 0, 0, 50, 50 };
    pixman_image_fill_rectangles(PIXMAN_OP_SRC, src, &red, 1, &src_rect);

    // 3. Composite the red square OVER the blue background at (25, 25)
    pixman_image_composite(PIXMAN_OP_OVER,
                           src,   // source
                           nullptr,  // mask
                           dest,  // destination
                           0, 0,  // src x,y
                           0, 0,  // mask x,y
                           25, 25, // dest x,y
                           50, 50  // width, height
    );

    // 4. Verification: Check a pixel in the centre (should be red)
    uint32_t *pixels = pixman_image_get_data(dest);
    uint32_t centre_pixel = pixels[50 * width + 50];

    if (centre_pixel == 0xFFFF0000)
        printf("Success: Centre pixel is red!\n");
    else
        printf("Failure: Centre pixel is 0x%08X\n", centre_pixel);

    // Cleanup
    pixman_image_unref(src);
    pixman_image_unref(dest);

    return 0;
}
