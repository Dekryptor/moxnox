module;

#include <stdint.h>
#include <immintrin.h>
#include <algorithm>

module framebuffer;

using namespace std;

static void transpose_16x16_avx512(__m512i* rows_out, const __m512i* rows_in,
                                   bool reverse) {
    alignas(64) static const uint32_t reverse_idx[16] = {
        15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
    };
    __m512i rev_idx = _mm512_load_si512((__m512i*)reverse_idx);

    __m512i r0 = rows_in[0];
    __m512i r1 = rows_in[1];
    __m512i r2 = rows_in[2];
    __m512i r3 = rows_in[3];
    __m512i r4 = rows_in[4];
    __m512i r5 = rows_in[5];
    __m512i r6 = rows_in[6];
    __m512i r7 = rows_in[7];
    __m512i r8 = rows_in[8];
    __m512i r9 = rows_in[9];
    __m512i r10 = rows_in[10];
    __m512i r11 = rows_in[11];
    __m512i r12 = rows_in[12];
    __m512i r13 = rows_in[13];
    __m512i r14 = rows_in[14];
    __m512i r15 = rows_in[15];

    __m512i t0 = _mm512_unpacklo_epi32(r0, r1);
    __m512i t1 = _mm512_unpackhi_epi32(r0, r1);
    __m512i t2 = _mm512_unpacklo_epi32(r2, r3);
    __m512i t3 = _mm512_unpackhi_epi32(r2, r3);
    __m512i t4 = _mm512_unpacklo_epi32(r4, r5);
    __m512i t5 = _mm512_unpackhi_epi32(r4, r5);
    __m512i t6 = _mm512_unpacklo_epi32(r6, r7);
    __m512i t7 = _mm512_unpackhi_epi32(r6, r7);
    __m512i t8 = _mm512_unpacklo_epi32(r8, r9);
    __m512i t9 = _mm512_unpackhi_epi32(r8, r9);
    __m512i t10 = _mm512_unpacklo_epi32(r10, r11);
    __m512i t11 = _mm512_unpackhi_epi32(r10, r11);
    __m512i t12 = _mm512_unpacklo_epi32(r12, r13);
    __m512i t13 = _mm512_unpackhi_epi32(r12, r13);
    __m512i t14 = _mm512_unpacklo_epi32(r14, r15);
    __m512i t15 = _mm512_unpackhi_epi32(r14, r15);

    __m512i u0 = _mm512_unpacklo_epi64(t0, t2);
    __m512i u1 = _mm512_unpackhi_epi64(t0, t2);
    __m512i u2 = _mm512_unpacklo_epi64(t1, t3);
    __m512i u3 = _mm512_unpackhi_epi64(t1, t3);
    __m512i u4 = _mm512_unpacklo_epi64(t4, t6);
    __m512i u5 = _mm512_unpackhi_epi64(t4, t6);
    __m512i u6 = _mm512_unpacklo_epi64(t5, t7);
    __m512i u7 = _mm512_unpackhi_epi64(t5, t7);
    __m512i u8 = _mm512_unpacklo_epi64(t8, t10);
    __m512i u9 = _mm512_unpackhi_epi64(t8, t10);
    __m512i u10 = _mm512_unpacklo_epi64(t9, t11);
    __m512i u11 = _mm512_unpackhi_epi64(t9, t11);
    __m512i u12 = _mm512_unpacklo_epi64(t12, t14);
    __m512i u13 = _mm512_unpackhi_epi64(t12, t14);
    __m512i u14 = _mm512_unpacklo_epi64(t13, t15);
    __m512i u15 = _mm512_unpackhi_epi64(t13, t15);

    __m512i v0 = _mm512_shuffle_i32x4(u0, u4, 0x88);
    __m512i v1 = _mm512_shuffle_i32x4(u1, u5, 0x88);
    __m512i v2 = _mm512_shuffle_i32x4(u2, u6, 0x88);
    __m512i v3 = _mm512_shuffle_i32x4(u3, u7, 0x88);
    __m512i v4 = _mm512_shuffle_i32x4(u0, u4, 0xdd);
    __m512i v5 = _mm512_shuffle_i32x4(u1, u5, 0xdd);
    __m512i v6 = _mm512_shuffle_i32x4(u2, u6, 0xdd);
    __m512i v7 = _mm512_shuffle_i32x4(u3, u7, 0xdd);
    __m512i v8 = _mm512_shuffle_i32x4(u8, u12, 0x88);
    __m512i v9 = _mm512_shuffle_i32x4(u9, u13, 0x88);
    __m512i v10 = _mm512_shuffle_i32x4(u10, u14, 0x88);
    __m512i v11 = _mm512_shuffle_i32x4(u11, u15, 0x88);
    __m512i v12 = _mm512_shuffle_i32x4(u8, u12, 0xdd);
    __m512i v13 = _mm512_shuffle_i32x4(u9, u13, 0xdd);
    __m512i v14 = _mm512_shuffle_i32x4(u10, u14, 0xdd);
    __m512i v15 = _mm512_shuffle_i32x4(u11, u15, 0xdd);

    __m512i w0 = _mm512_shuffle_i32x4(v0, v8, 0x88);
    __m512i w1 = _mm512_shuffle_i32x4(v1, v9, 0x88);
    __m512i w2 = _mm512_shuffle_i32x4(v2, v10, 0x88);
    __m512i w3 = _mm512_shuffle_i32x4(v3, v11, 0x88);
    __m512i w4 = _mm512_shuffle_i32x4(v4, v12, 0x88);
    __m512i w5 = _mm512_shuffle_i32x4(v5, v13, 0x88);
    __m512i w6 = _mm512_shuffle_i32x4(v6, v14, 0x88);
    __m512i w7 = _mm512_shuffle_i32x4(v7, v15, 0x88);
    __m512i w8 = _mm512_shuffle_i32x4(v0, v8, 0xdd);
    __m512i w9 = _mm512_shuffle_i32x4(v1, v9, 0xdd);
    __m512i w10 = _mm512_shuffle_i32x4(v2, v10, 0xdd);
    __m512i w11 = _mm512_shuffle_i32x4(v3, v11, 0xdd);
    __m512i w12 = _mm512_shuffle_i32x4(v4, v12, 0xdd);
    __m512i w13 = _mm512_shuffle_i32x4(v5, v13, 0xdd);
    __m512i w14 = _mm512_shuffle_i32x4(v6, v14, 0xdd);
    __m512i w15 = _mm512_shuffle_i32x4(v7, v15, 0xdd);

    if (reverse) {
        rows_out[0] = _mm512_permutexvar_epi32(rev_idx, w0);
        rows_out[1] = _mm512_permutexvar_epi32(rev_idx, w1);
        rows_out[2] = _mm512_permutexvar_epi32(rev_idx, w2);
        rows_out[3] = _mm512_permutexvar_epi32(rev_idx, w3);
        rows_out[4] = _mm512_permutexvar_epi32(rev_idx, w4);
        rows_out[5] = _mm512_permutexvar_epi32(rev_idx, w5);
        rows_out[6] = _mm512_permutexvar_epi32(rev_idx, w6);
        rows_out[7] = _mm512_permutexvar_epi32(rev_idx, w7);
        rows_out[8] = _mm512_permutexvar_epi32(rev_idx, w8);
        rows_out[9] = _mm512_permutexvar_epi32(rev_idx, w9);
        rows_out[10] = _mm512_permutexvar_epi32(rev_idx, w10);
        rows_out[11] = _mm512_permutexvar_epi32(rev_idx, w11);
        rows_out[12] = _mm512_permutexvar_epi32(rev_idx, w12);
        rows_out[13] = _mm512_permutexvar_epi32(rev_idx, w13);
        rows_out[14] = _mm512_permutexvar_epi32(rev_idx, w14);
        rows_out[15] = _mm512_permutexvar_epi32(rev_idx, w15);
    } else {
        rows_out[0] = w0;
        rows_out[1] = w1;
        rows_out[2] = w2;
        rows_out[3] = w3;
        rows_out[4] = w4;
        rows_out[5] = w5;
        rows_out[6] = w6;
        rows_out[7] = w7;
        rows_out[8] = w8;
        rows_out[9] = w9;
        rows_out[10] = w10;
        rows_out[11] = w11;
        rows_out[12] = w12;
        rows_out[13] = w13;
        rows_out[14] = w14;
        rows_out[15] = w15;
    }
}

void flush_scanlines_rotated90_avx512(uint32_t min_y, uint32_t max_y) {
    min_y &= (uint32_t)~15;
    max_y = min(shadow_fb_height - 1, (uint32_t)((max_y + 15) & (uint32_t)~15));

    for (uint32_t block_y = min_y; block_y <= max_y; block_y += 16) {
        for (uint32_t block_x = 0; block_x < shadow_fb_width; block_x += 16) {
            __m512i rows_in[16];

            for (unsigned int i = 0; i < 16; i++) {
                uint32_t src_y = block_y + i;

                auto shadow_line = &shadow_framebuffer[src_y * shadow_fb_pixels_per_scan_line];

                rows_in[i] = _mm512_loadu_si512((__m512i*)&shadow_line[block_x]);
            }

            __m512i rows_out[16];
            transpose_16x16_avx512(rows_out, rows_in, true);

            uint32_t dest_x = shadow_fb_height - 16 - block_y;

            for (unsigned int i = 0; i < 16; i++) {
                uint32_t dest_y = block_x + i;

                if (dest_y >= fb_height)
                    continue;

                auto fb_line = &framebuffer[dest_y  * fb_pixels_per_scan_line];

                _mm512_storeu_si512((__m256i*)&fb_line[dest_x], rows_out[i]);
            }
        }
    }
}

void flush_scanlines_rotated270_avx512(uint32_t min_y, uint32_t max_y) {
    min_y &= (uint32_t)~15;
    max_y = min(shadow_fb_height - 1, (uint32_t)((max_y + 15) & (uint32_t)~15));

    for (uint32_t block_y = min_y; block_y <= max_y; block_y += 16) {
        for (uint32_t block_x = 0; block_x < shadow_fb_width; block_x += 16) {
            __m512i rows_in[16];

            for (unsigned int i = 0; i < 16; i++) {
                uint32_t src_y = block_y + i;

                auto shadow_line = &shadow_framebuffer[src_y * shadow_fb_pixels_per_scan_line];

                rows_in[i] = _mm512_loadu_si512((__m512i*)&shadow_line[block_x]);
            }

            __m512i rows_out[16];
            transpose_16x16_avx512(rows_out, rows_in, false);

            uint32_t dest_x = block_y;

            for (unsigned int i = 0; i < 16; i++) {
                uint32_t dest_y = shadow_fb_width - 1 - block_x - i;

                if (dest_y >= fb_height)
                    continue;

                auto fb_line = &framebuffer[dest_y  * fb_pixels_per_scan_line];

                _mm512_storeu_si512((__m256i*)&fb_line[dest_x], rows_out[i]);
            }
        }
    }
}
