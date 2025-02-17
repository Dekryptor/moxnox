.intel_syntax noprefix

#include "context.S"
#include "dwarf.S"

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

.uleb128 2
.asciz "sata_isr"
.quad sata_isr
.quad sata_isr_end - sata_isr

.uleb128 0

.debug_info_end:

.text

.seh_proc sata_isr
.global sata_isr
sata_isr:
    .seh_pushframe
    sub rsp, 0x58
    .seh_stackalloc 0x58
    .seh_endprologue

    cmp QWORD PTR [rsp+0x60], 0x8
    je 0f
    swapgs
    load_kernel_cr3
0:
    mov [rsp+0x50], rax
    mov [rsp+0x48], rcx
    mov [rsp+0x40], rdx
    mov [rsp+0x38], r8
    mov [rsp+0x30], r9
    mov [rsp+0x28], r10
    mov [rsp+0x20], r11

    call sata_isr_func

    xor rax, rax
    mov DWORD PTR [APIC_BASE+APIC_EOI], eax

    mov r11, [rsp+0x20]
    mov r10, [rsp+0x28]
    mov r9, [rsp+0x30]
    mov r8, [rsp+0x38]
    mov rdx, [rsp+0x40]
    mov rcx, [rsp+0x48]
    mov rax, [rsp+0x50]

    cmp QWORD PTR [rsp+0x60], 0x8
    je 1f
    load_process_cr3
    swapgs
1:
    add rsp, 0x58
    iretq
    .seh_endproc
sata_isr_end:

.text_end:
