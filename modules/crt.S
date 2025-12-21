.intel_syntax noprefix

#include "../kernel/src/dwarf.S"

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

dwarf_func_asm memcpy

.uleb128 0

.debug_info_end:

.text

# void* memcpy(void* dest, const void* src, size_t n)
.global memcpy
memcpy:
    # rcx = dest, rdx = src, r8 = n
    mov r9, rcx

    # FIXME - SSE?

0:
    cmp r8, 8
    jl 1f
    mov rax, [rdx]
    mov [rcx], rax
    add rdx, 8
    add rcx, 8
    sub r8, 8
    jmp 0b

1:
    cmp r8, 4
    jl 2f
    mov eax, [rdx]
    mov [rcx], eax
    add rdx, 4
    add rcx, 4
    sub r8, 4
    jmp 1b

2:
    cmp r8, 2
    jl 3f
    mov ax, [rdx]
    mov [rcx], ax
    add rdx, 2
    add rcx, 2
    sub r8, 2
    jmp 2b

3:
    test r8, r8
    je 4f
    mov al, [rdx]
    mov [rcx], al
    inc rdx
    inc rcx
    dec r8
    jmp 3b

4:
    mov rax, r9
    ret
memcpy_end:

.text_end:
