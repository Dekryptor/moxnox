.intel_syntax noprefix

#include "../dwarf.S"
#include "../exception_record_offsets.h"

.macro stubs_start
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

    .text
.endm

.macro do_stub_no_export name
    .section .rdata
    \name\()_name:
    .asciz "\name"

    .text
    .global "\name"
    .seh_proc "\name"
    "\name":

    sub rsp, 0x28
    .seh_stackalloc 0x28
    .seh_endprologue

    # EXCEPTION_RECORD* er is at rsp

    mov DWORD PTR [rsp+EXCEPTION_RECORD_EXCEPTION_CODE], MOXNOX_STUB_FUNCTION
    mov DWORD PTR [rsp+EXCEPTION_RECORD_EXCEPTION_FLAGS], 0
    mov QWORD PTR [rsp+EXCEPTION_RECORD_EXCEPTION_RECORD], 0
    mov QWORD PTR [rsp+EXCEPTION_RECORD_EXCEPTION_ADDRESS], 0
    mov QWORD PTR [rsp+EXCEPTION_RECORD_NUMBER_PARAMETERS], 1
    lea rdx, [rip + \name\()_name]
    mov [rsp+EXCEPTION_RECORD_EXCEPTION_INFORMATION], rdx

    mov rcx, rsp
    call RtlRaiseException
    nop # to make stacktrace look nice

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
