/* SPDX-License-Identifier: GPL-2.0-only */
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
dwarf_func_asm copy_scanline_avx2
dwarf_func_asm copy_scanline_mirrored_avx512
dwarf_func_asm copy_scanline_mirrored_avx2

.uleb128 0

.debug_info_end:

.section ".rdata", "r"

.p2align 5
mirrored_avx2_permute:
.long 7
.long 6
.long 5
.long 4
.long 3
.long 2
.long 1
.long 0

.p2align 6
mirrored_avx512_permute:
.long 15
.long 14
.long 13
.long 12
.long 11
.long 10
.long 9
.long 8
.long 7
.long 6
.long 5
.long 4
.long 3
.long 2
.long 1
.long 0

.text

# void copy_scanline_avx512(void* dst, const void* src, size_t n);
# rcx = dst, rdx = src, r8 = n
.global copy_scanline_avx512
copy_scanline_avx512:
    # using the six volatile AVX512 registers, so we don't have to save and
    # restore anything because of the calling convention

    cmp r8, 384
    jb .Lloop_one_reg_avx512

.Lloop_multi_reg_avx512:
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
    jae .Lloop_multi_reg_avx512

    cmp r8, 64
    jb .Lend_avx512

.Lloop_one_reg_avx512:
    vmovdqa64 zmm0, [rdx]
    vmovntdq [rcx], zmm0
    add rdx, 64
    add rcx, 64
    sub r8, 64
    cmp r8, 64
    jae .Lloop_one_reg_avx512

.Lend_avx512:
    sfence

    ret
copy_scanline_avx512_end:

# void copy_scanline_avx2(void* dst, const void* src, size_t n);
# rcx = dst, rdx = src, r8 = n
.global copy_scanline_avx2
copy_scanline_avx2:
    # using the six volatile AVX2 registers, so we don't have to save and
    # restore anything because of the calling convention

    cmp r8, 192
    jb .Lloop_one_reg_avx2

.Lloop_multi_reg_avx2:
    # FIXME - prefetch?

    vmovdqa ymm0, [rdx]
    vmovdqa ymm1, [rdx + 32]
    vmovdqa ymm2, [rdx + 64]
    vmovdqa ymm3, [rdx + 96]
    vmovdqa ymm4, [rdx + 128]
    vmovdqa ymm5, [rdx + 160]

    vmovntdq [rcx], ymm0
    vmovntdq [rcx + 32], ymm1
    vmovntdq [rcx + 64], ymm2
    vmovntdq [rcx + 96], ymm3
    vmovntdq [rcx + 128], ymm4
    vmovntdq [rcx + 160], ymm5

    add rdx, 192
    add rcx, 192
    sub r8, 192
    cmp r8, 192
    jae .Lloop_multi_reg_avx2

    cmp r8, 64
    jb .Lend_avx2

.Lloop_one_reg_avx2:
    vmovdqa ymm0, [rdx]
    vmovntdq [rcx], ymm0
    add rdx, 32
    add rcx, 32
    sub r8, 32
    cmp r8, 32
    jae .Lloop_one_reg_avx2

.Lend_avx2:
    sfence

    ret
copy_scanline_avx2_end:

# void copy_scanline_mirrored_avx512(uint32_t* dst, const uint32_t* src,
#                                    uint64_t x, uint64_t length);
# rcx = dst, rdx = src, r8 = x, r9 = length
# rax = source pointer, r10 = destination pointer
.global copy_scanline_mirrored_avx512
copy_scanline_mirrored_avx512:
    lea rax, [rdx+(r8*4)]

    mov r10d, DWORD PTR [rip+fb_pixels_per_scan_line]
    sub r10, r8
    sub r10, 16
    lea r10, [rcx+(r10*4)]

    vmovdqa64 zmm1, [rip+mirrored_avx512_permute]

0:
    vmovdqa64 zmm0, [rax]
    vpermd zmm0, zmm1, zmm0
    vmovntdq [r10], zmm0

    sub r10, 64
    add rax, 64
    sub r9, 16
    cmp r9, 16
    jae 0b

    sfence
    ret
copy_scanline_mirrored_avx512_end:

# void copy_scanline_mirrored_avx2(uint32_t* dst, const uint32_t* src,
#                                  uint64_t x, uint64_t length);
# rcx = dst, rdx = src, r8 = x, r9 = length
# rax = source pointer, r10 = destination pointer
.global copy_scanline_mirrored_avx2
copy_scanline_mirrored_avx2:
    lea rax, [rdx+(r8*4)]

    mov r10d, DWORD PTR [rip+fb_pixels_per_scan_line]
    sub r10, r8
    sub r10, 8
    lea r10, [rcx+(r10*4)]

    vmovdqa ymm1, [rip+mirrored_avx2_permute]

0:
    vmovdqa ymm0, [rax]
    vpermd ymm0, ymm1, ymm0
    vmovntdq [r10], ymm0

    sub r10, 32
    add rax, 32
    sub r9, 8
    cmp r9, 8
    jae 0b

    sfence
    ret
copy_scanline_mirrored_avx2_end:

.text_end:
