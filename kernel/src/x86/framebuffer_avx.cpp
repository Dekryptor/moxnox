module;

#include <stdint.h>
#include <immintrin.h>
#include <algorithm>

module framebuffer;

using namespace std;

static void transpose_8x8_avx2(__m256i* rows_out, const __m256i* rows_in,
                               bool reverse) {
    alignas(32) static const uint32_t reverse_idx[8] = {
        7, 6, 5, 4, 3, 2, 1, 0
    };
    __m256i rev_idx = _mm256_load_si256((__m256i*)reverse_idx);

    __m256i r0 = rows_in[0];
    __m256i r1 = rows_in[1];
    __m256i r2 = rows_in[2];
    __m256i r3 = rows_in[3];
    __m256i r4 = rows_in[4];
    __m256i r5 = rows_in[5];
    __m256i r6 = rows_in[6];
    __m256i r7 = rows_in[7];

    __m256i t0 = _mm256_unpacklo_epi32(r0, r1);
    __m256i t1 = _mm256_unpackhi_epi32(r0, r1);
    __m256i t2 = _mm256_unpacklo_epi32(r2, r3);
    __m256i t3 = _mm256_unpackhi_epi32(r2, r3);
    __m256i t4 = _mm256_unpacklo_epi32(r4, r5);
    __m256i t5 = _mm256_unpackhi_epi32(r4, r5);
    __m256i t6 = _mm256_unpacklo_epi32(r6, r7);
    __m256i t7 = _mm256_unpackhi_epi32(r6, r7);

    __m256i u0 = _mm256_unpacklo_epi64(t0, t2);
    __m256i u1 = _mm256_unpackhi_epi64(t0, t2);
    __m256i u2 = _mm256_unpacklo_epi64(t1, t3);
    __m256i u3 = _mm256_unpackhi_epi64(t1, t3);
    __m256i u4 = _mm256_unpacklo_epi64(t4, t6);
    __m256i u5 = _mm256_unpackhi_epi64(t4, t6);
    __m256i u6 = _mm256_unpacklo_epi64(t5, t7);
    __m256i u7 = _mm256_unpackhi_epi64(t5, t7);

    __m256i v0 = _mm256_permute2x128_si256(u0, u4, 0x20);
    __m256i v1 = _mm256_permute2x128_si256(u1, u5, 0x20);
    __m256i v2 = _mm256_permute2x128_si256(u2, u6, 0x20);
    __m256i v3 = _mm256_permute2x128_si256(u3, u7, 0x20);
    __m256i v4 = _mm256_permute2x128_si256(u0, u4, 0x31);
    __m256i v5 = _mm256_permute2x128_si256(u1, u5, 0x31);
    __m256i v6 = _mm256_permute2x128_si256(u2, u6, 0x31);
    __m256i v7 = _mm256_permute2x128_si256(u3, u7, 0x31);

    if (reverse) {
        rows_out[0] = _mm256_permutevar8x32_epi32(v0, rev_idx);
        rows_out[1] = _mm256_permutevar8x32_epi32(v1, rev_idx);
        rows_out[2] = _mm256_permutevar8x32_epi32(v2, rev_idx);
        rows_out[3] = _mm256_permutevar8x32_epi32(v3, rev_idx);
        rows_out[4] = _mm256_permutevar8x32_epi32(v4, rev_idx);
        rows_out[5] = _mm256_permutevar8x32_epi32(v5, rev_idx);
        rows_out[6] = _mm256_permutevar8x32_epi32(v6, rev_idx);
        rows_out[7] = _mm256_permutevar8x32_epi32(v7, rev_idx);
    } else {
        rows_out[0] = v0;
        rows_out[1] = v1;
        rows_out[2] = v2;
        rows_out[3] = v3;
        rows_out[4] = v4;
        rows_out[5] = v5;
        rows_out[6] = v6;
        rows_out[7] = v7;
    }
}

void flush_scanlines_rotated90_avx2(uint32_t* buffer, uint32_t min_y,
                                    uint32_t max_y) {
    min_y &= (uint32_t)~7;
    max_y = min(shadow_fb_height - 1, (uint32_t)((max_y + 7) & (uint32_t)~7));

    for (uint32_t block_y = min_y; block_y <= max_y; block_y += 8) {
        for (uint32_t block_x = 0; block_x < shadow_fb_width; block_x += 8) {
            __m256i rows_in[8];

            for (unsigned int i = 0; i < 8; i++) {
                uint32_t src_y = block_y + i;

                auto shadow_line = &buffer[src_y * shadow_fb_pixels_per_scan_line];

                rows_in[i] = _mm256_loadu_si256((__m256i*)&shadow_line[block_x]);
            }

            __m256i rows_out[8];
            transpose_8x8_avx2(rows_out, rows_in, true);

            uint32_t dest_x = shadow_fb_height - 8 - block_y;

            for (unsigned int i = 0; i < 8; i++) {
                uint32_t dest_y = block_x + i;

                if (dest_y >= fb_height)
                    continue;

                auto fb_line = &framebuffer[dest_y * fb_pixels_per_scan_line];

                _mm256_storeu_si256((__m256i*)&fb_line[dest_x], rows_out[i]);
            }
        }
    }
}

void flush_scanlines_rotated270_avx2(uint32_t* buffer, uint32_t min_y,
                                     uint32_t max_y) {
    min_y &= (uint32_t)~7;
    max_y = min(shadow_fb_height - 1, (uint32_t)((max_y + 7) & (uint32_t)~7));

    for (uint32_t block_y = min_y; block_y <= max_y; block_y += 8) {
        for (uint32_t block_x = 0; block_x < shadow_fb_width; block_x += 8) {
            __m256i rows_in[8];

            for (unsigned int i = 0; i < 8; i++) {
                uint32_t src_y = block_y + i;

                auto shadow_line = &buffer[src_y * shadow_fb_pixels_per_scan_line];

                rows_in[i] = _mm256_loadu_si256((__m256i*)&shadow_line[block_x]);
            }

            __m256i rows_out[8];
            transpose_8x8_avx2(rows_out, rows_in, false);

            uint32_t dest_x = block_y;

            for (unsigned int i = 0; i < 8; i++) {
                uint32_t dest_y = shadow_fb_width - 1 - block_x - i;

                if (dest_y >= fb_height)
                    continue;

                auto fb_line = &framebuffer[dest_y * fb_pixels_per_scan_line];

                _mm256_storeu_si256((__m256i*)&fb_line[dest_x], rows_out[i]);
            }
        }
    }
}
