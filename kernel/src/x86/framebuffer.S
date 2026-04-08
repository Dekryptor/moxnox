.intel_syntax noprefix

#include "../dwarf.S"

.section .debug_abbrev, "dr"

.uleb128 1 # entry num
.uleb128 DW_TAG_compile_unit
.byte 1 # has children
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

.uleb128 2 # entry num
.uleb128 DW_TAG_subprogram
.byte 0 # has children
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
.short 5 # version
.byte DW_UT_compile
.byte 8 # address size
.secrel32 .debug_abbrev

.uleb128 1
.quad .text # low_pc
.uleb128 .text_end - .text # high_pc
.asciz __FILE__ # name
.short DW_LANG_Mips_Assembler # language

dwarf_func_asm copy_scanline_avx512

.uleb128 0

.debug_info_end:

.text

# void copy_scanline_avx512(void* dst, const void* src, size_t n);
# rcx = dst, rdx = src, r8 = n
.global copy_scanline_avx512
copy_scanline_avx512:
    # using the six volatile AVX512 registers, so we don't have to save and
    # restore anything because of the calling convention

    cmp r8, 384
    jb .Lloop_one_reg

.Lloop_multi_reg:
    # FIXME - prefetch?

    vmovdqa64 zmm0, [rdx]
    vmovdqa64 zmm1, [rdx + 64]
    vmovdqa64 zmm2, [rdx + 128]
    vmovdqa64 zmm3, [rdx + 192]
    vmovdqa64 zmm4, [rdx + 256]
    vmovdqa64 zmm5, [rdx + 320]

    vmovntdq [rcx], zmm0
    vmovntdq [rcx + 64], zmm1
    vmovntdq [rcx + 128], zmm2
    vmovntdq [rcx + 192], zmm3
    vmovntdq [rcx + 256], zmm4
    vmovntdq [rcx + 320], zmm5

    add rdx, 384
    add rcx, 384
    sub r8, 384
    cmp r8, 384
    jae .Lloop_multi_reg

    sfence

.Lloop_one_reg:
    vmovdqa64 zmm0, [rdx]
    vmovntdq [rcx], zmm0
    add rdx, 64
    add rcx, 64
    sub r8, 64
    cmp r8, 64
    jae .Lloop_one_reg

    sfence

    ret
copy_scanline_avx512_end:

.text_end:
