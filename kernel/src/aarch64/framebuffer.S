/* SPDX-License-Identifier: GPL-2.0-only */
#include "../dwarf.S"

.section .debug_abbrev, "dr"

.uleb128 1 // entry num
.uleb128 DW_TAG_compile_unit
.byte 1 // has children
.uleb128 DW_AT_low_pc
.uleb128 DW_FORM_addr
.uleb128 DW_AT_high_pc
.uleb128 DW_FORM_udata
.uleb128 DW_AT_name
.uleb128 DW_FORM_string
.uleb128 DW_AT_language
.uleb128 DW_FORM_data2
.uleb128 0
.uleb128 0

.uleb128 2 // entry num
.uleb128 DW_TAG_subprogram
.byte 0 // has children
.uleb128 DW_AT_name
.uleb128 DW_FORM_string
.uleb128 DW_AT_low_pc
.uleb128 DW_FORM_addr
.uleb128 DW_AT_high_pc
.uleb128 DW_FORM_data8
.uleb128 0
.uleb128 0

.uleb128 0

.section .debug_info, "dr"

.long .debug_info_end - .debug_info_start

.debug_info_start:
.short 5 // version
.byte DW_UT_compile
.byte 8 // address size
.secrel32 .debug_abbrev

.uleb128 1
.quad .text // low_pc
.uleb128 .text_end - .text // high_pc
.asciz __FILE__ // name
.short DW_LANG_Mips_Assembler // language

dwarf_func_asm copy_scanline_neon
dwarf_func_asm copy_scanline_mirrored_neon

.uleb128 0

.debug_info_end:

.section ".rdata", "r"

.p2align 4
mirrored_neon_permute:
.byte 12
.byte 13
.byte 14
.byte 15
.byte 8
.byte 9
.byte 10
.byte 11
.byte 4
.byte 5
.byte 6
.byte 7
.byte 0
.byte 1
.byte 2
.byte 3

.text

# void copy_scanline_neon(void* dst, const void* src, size_t n);
# x0 = dst, x1 = src, x2 = n
.global copy_scanline_neon
copy_scanline_neon:
    cmp x2, 128
    b.lt .Lloop_one_reg_neon

.Lloop_multi_reg_neon:
    ldp q16, q17, [x1]
    ldp q18, q19, [x1, 32]
    ldp q20, q21, [x1, 64]
    ldp q22, q23, [x1, 96]

    stp q16, q17, [x0]
    stp q18, q19, [x0, 32]
    stp q20, q21, [x0, 64]
    stp q22, q23, [x0, 96]

    add x0, x0, 128
    add x1, x1, 128
    sub x2, x2, 128

    cmp x2, 127
    b.hi .Lloop_multi_reg_neon

.Lloop_one_reg_neon:
    cmp x2, 16
    b.lt .Lend_avx512

    ldr q16, [x1]
    str q16, [x0]

    add x0, x0, 16
    add x1, x1, 16
    sub x2, x2, 16

    b .Lloop_one_reg_neon

.Lend_avx512:
    ret
copy_scanline_neon_end:

# void copy_scanline_mirrored_neon(uint32_t* dst, const uint32_t* src,
#                                  uint64_t x, uint64_t length);
# x0 = dst, x1 = src, x2 = x, x3 = length
# x4 = source pointer, x5 = destination pointer
.global copy_scanline_mirrored_neon
copy_scanline_mirrored_neon:
    add x4, x1, x2, lsl 2

    adrp x5, fb_pixels_per_scan_line
    ldr w5, [x5, #:lo12:fb_pixels_per_scan_line]
    sub x5, x5, x2
    sub x5, x5, 4
    add x5, x0, x5, lsl 2

    adrp x6, mirrored_neon_permute
    ldr q17, [x6, #:lo12:mirrored_neon_permute]

0:
    ldr q16, [x4]
    tbl v16.16b, {v16.16b}, v17.16b
    str q16, [x5]

    sub x5, x5, 16
    add x4, x4, 16
    sub x3, x3, 4
    cmp x3, 4
    b.ge 0b

    ret
copy_scanline_mirrored_neon_end:

.text_end:
