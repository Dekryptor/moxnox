.intel_syntax noprefix

#include "../setjmp_offsets.h"
#include "../../modules/dwarf.S"

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

dwarf_func_asm _setjmp

.uleb128 0

.debug_info_end:

.text

# int _setjmp(jmp_buf env, void* frame);
# rcx = env, rdx = frame
.global _setjmp
_setjmp:
    mov [rcx + JUMP_BUFFER_FRAME], rdx
    mov [rcx + JUMP_BUFFER_RBX], rbx

    lea rax, [rsp + 8] # return address stack
    mov [rcx + JUMP_BUFFER_RSP], rax

    mov [rcx + JUMP_BUFFER_RBP], rbp
    mov [rcx + JUMP_BUFFER_RSI], rsi
    mov [rcx + JUMP_BUFFER_RDI], rdi
    mov [rcx + JUMP_BUFFER_R12], r12
    mov [rcx + JUMP_BUFFER_R13], r13
    mov [rcx + JUMP_BUFFER_R14], r14
    mov [rcx + JUMP_BUFFER_R15], r15

    mov rax, [rsp] # return address
    mov [rcx + JUMP_BUFFER_RIP], rax

    stmxcsr DWORD PTR [rcx + JUMP_BUFFER_MXCSR]
    fnstcw WORD PTR [rcx + JUMP_BUFFER_FPCSR]

    movaps [rcx + JUMP_BUFFER_XMM6], xmm6
    movaps [rcx + JUMP_BUFFER_XMM7], xmm7
    movaps [rcx + JUMP_BUFFER_XMM8], xmm8
    movaps [rcx + JUMP_BUFFER_XMM9], xmm9
    movaps [rcx + JUMP_BUFFER_XMM10], xmm10
    movaps [rcx + JUMP_BUFFER_XMM11], xmm11
    movaps [rcx + JUMP_BUFFER_XMM12], xmm12
    movaps [rcx + JUMP_BUFFER_XMM13], xmm13
    movaps [rcx + JUMP_BUFFER_XMM14], xmm14
    movaps [rcx + JUMP_BUFFER_XMM15], xmm15

    xor rax, rax
    ret
_setjmp_end:

.text_end:
