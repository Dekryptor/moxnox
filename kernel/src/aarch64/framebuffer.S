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

.uleb128 0

.debug_info_end:

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

.text_end:
