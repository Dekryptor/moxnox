/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "../setjmp_offsets.h"
#include "../../modules/dwarf.S"

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

dwarf_func_asm _setjmp

.uleb128 0

.debug_info_end:

.text

# int _setjmp(jmp_buf env, void* frame);
# x0 = env, x1 = frame
.global _setjmp
_setjmp:
    str x1, [x0, #JUMP_BUFFER_FRAME]
    stp x19, x20, [x0, #JUMP_BUFFER_X19]
    stp x21, x22, [x0, #JUMP_BUFFER_X21]
    stp x23, x24, [x0, #JUMP_BUFFER_X23]
    stp x25, x26, [x0, #JUMP_BUFFER_X25]
    stp x27, x28, [x0, #JUMP_BUFFER_X27]
    stp fp, lr, [x0, #JUMP_BUFFER_FP]

    mov x1, sp
    str x1, [x0, #JUMP_BUFFER_SP]

    mrs x1, fpcr
    str w1, [x0, #JUMP_BUFFER_FPCR]
    mrs x1, fpsr
    str w1, [x0, #JUMP_BUFFER_FPSR]

    stp d8, d9, [x0, #JUMP_BUFFER_D]
    stp d10, d11, [x0, #JUMP_BUFFER_D + 0x10]
    stp d12, d13, [x0, #JUMP_BUFFER_D + 0x20]
    stp d14, d15, [x0, #JUMP_BUFFER_D + 0x30]

    mov x0, xzr
    ret
_setjmp_end:

.text_end:
