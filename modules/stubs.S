.intel_syntax noprefix

#include "dwarf.S"
#include "exception_record_offsets.h"

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

.macro do_stub name
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

    # #define EXCEPTION_RECORD_EXCEPTION_CODE 0x0
    # #define EXCEPTION_RECORD_EXCEPTION_FLAGS 0x4
    # #define EXCEPTION_RECORD_EXCEPTION_RECORD 0x8
    # #define EXCEPTION_RECORD_EXCEPTION_ADDRESS 0x10
    # #define EXCEPTION_RECORD_NUMBER_PARAMETERS 0x18
    # #define EXCEPTION_RECORD_EXCEPTION_INFORMATION 0x20

    # typedef struct _EXCEPTION_RECORD {
    #     DWORD                    ExceptionCode;
    #     DWORD                    ExceptionFlags;
    #     struct _EXCEPTION_RECORD *ExceptionRecord;
    #     PVOID                    ExceptionAddress;
    #     DWORD                    NumberParameters;
    #     ULONG_PTR                ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
    # } EXCEPTION_RECORD;

#     EXCEPTION_RECORD er;

    mov DWORD PTR [rsp+EXCEPTION_RECORD_EXCEPTION_CODE], MOXNOX_STUB_FUNCTION
    mov DWORD PTR [rsp+EXCEPTION_RECORD_EXCEPTION_FLAGS], 0
    mov QWORD PTR [rsp+EXCEPTION_RECORD_EXCEPTION_RECORD], 0
    mov QWORD PTR [rsp+EXCEPTION_RECORD_EXCEPTION_ADDRESS], 0
    mov QWORD PTR [rsp+EXCEPTION_RECORD_NUMBER_PARAMETERS], 1
    lea rdx, [rip + \name\()_name]
    mov [rsp+EXCEPTION_RECORD_EXCEPTION_INFORMATION], rdx

    mov rcx, rsp
    call RtlRaiseException

#
#     er.ExceptionFlags = 0;
#     er.ExceptionRecord = nullptr;
#     // er.ExceptionAddress = (void*)ctx.Rip;
#     er.ExceptionCode = 0x12345678;
#     er.NumberParameters = 0;
#
#     print("before exception\n");
#
#     RtlRaiseException(&er);
    # call RtlRaiseException

    ud2
    .seh_endproc
    \name\()_end:

    .section .drectve
    .ascii " -export:\"\name\""

    .section .debug_info, "dr"
    dwarf_func_asm \name
.endm

.macro stubs_end
    .text
    .text_end:

    .section .debug_info, "dr"
    .uleb128 0
    .debug_info_end:
.endm
