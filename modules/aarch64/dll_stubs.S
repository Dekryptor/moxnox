/* SPDX-License-Identifier: MIT */
#include "../dwarf.S"
#include "../exception_record_offsets.h"

.macro stubs_start
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
.endm

.macro do_stub_no_export name
    .section .rdata
    \name\()_name:
    .asciz "\name"

    .text
    .global "\name"
    .seh_proc "\name"
    "\name":

    stp fp, lr, [sp, #-0xb0]!
    .seh_save_fplr_x 0xb0
    mov fp, sp
    .seh_set_fp
    .seh_endprologue

    add x0, sp, #0x10

    mov w1, #(MOXNOX_STUB_FUNCTION & 0xFFFF)
    movk w1, #((MOXNOX_STUB_FUNCTION >> 16) & 0xFFFF), lsl #16
    str w1, [x0, #EXCEPTION_RECORD_EXCEPTION_CODE]

    str wzr, [x0, #EXCEPTION_RECORD_EXCEPTION_FLAGS]
    str xzr, [x0, #EXCEPTION_RECORD_EXCEPTION_RECORD]
    str xzr, [x0, #EXCEPTION_RECORD_EXCEPTION_ADDRESS]
    mov x1, #1
    str x1, [x0, #EXCEPTION_RECORD_NUMBER_PARAMETERS]

    adr x1, \name\()_name
    str x1, [x0, #EXCEPTION_RECORD_EXCEPTION_INFORMATION]

    bl RtlRaiseException
    nop /* to make stacktrace look nice */

    .seh_endproc
    \name\()_end:

    .section .debug_info, "dr"
    dwarf_func_asm \name
.endm

.macro do_stub name
    do_stub_no_export \name

    .section .drectve
    .ascii " -export:\"\name\""
.endm

.macro stubs_end
    .text
    .text_end:

    .section .debug_info, "dr"
    .uleb128 0
    .debug_info_end:
.endm
