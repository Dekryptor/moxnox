.intel_syntax noprefix

#include "context.S"
#include "../dwarf.S"
#include "../context_offsets.h"

.equ IA32_PAT_MSR, 0x277

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
dwarf_func_asm memcpy_aligned
dwarf_func_asm init_pat

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

# void memcpy_aligned(void* dest, const void* src, size_t n)
# requires src and dest both to be aligned to 16 bytes
.global memcpy_aligned
memcpy_aligned:
    # rcx = dest, rdx = src, r8 = n
    cmp r8, 0x100
    jl memcpy
    prefetchnta [rdx+0x100]
    prefetchnta [rdx+0x120]
    prefetchnta [rdx+0x140]
    prefetchnta [rdx+0x160]
    prefetchnta [rdx+0x180]
    prefetchnta [rdx+0x1a0]
    prefetchnta [rdx+0x1c0]
    prefetchnta [rdx+0x1e0]
    movdqa xmm0, [rdx]
    movdqa xmm1, [rdx+0x10]
    movdqa xmm2, [rdx+0x20]
    movdqa xmm3, [rdx+0x30]
    movdqa xmm4, [rdx+0x40]
    movdqa xmm5, [rdx+0x50]
    movdqa xmm6, [rdx+0x60]
    movdqa xmm7, [rdx+0x70]
    movdqa xmm8, [rdx+0x80]
    movdqa xmm9, [rdx+0x90]
    movdqa xmm10, [rdx+0xa0]
    movdqa xmm11, [rdx+0xb0]
    movdqa xmm12, [rdx+0xc0]
    movdqa xmm13, [rdx+0xd0]
    movdqa xmm14, [rdx+0xe0]
    movdqa xmm15, [rdx+0xf0]
    movntdq [rcx], xmm0
    movntdq [rcx+0x10], xmm1
    movntdq [rcx+0x20], xmm2
    movntdq [rcx+0x30], xmm3
    movntdq [rcx+0x40], xmm4
    movntdq [rcx+0x50], xmm5
    movntdq [rcx+0x60], xmm6
    movntdq [rcx+0x70], xmm7
    movntdq [rcx+0x80], xmm8
    movntdq [rcx+0x90], xmm9
    movntdq [rcx+0xa0], xmm10
    movntdq [rcx+0xb0], xmm11
    movntdq [rcx+0xc0], xmm12
    movntdq [rcx+0xd0], xmm13
    movntdq [rcx+0xe0], xmm14
    movntdq [rcx+0xf0], xmm15
    add rdx, 0x100
    add rcx, 0x100
    sub r8, 0x100
    jmp memcpy_aligned
memcpy_aligned_end:

.global init_pat
init_pat:
    # FIXME - do we need to check this is available, or is it there for all amd64 CPUs?

    # set PA4 to write-combine
    mov ecx, IA32_PAT_MSR
    rdmsr
    and edx, ~0x7
    or edx, 1
    wrmsr

    # force refresh
    mov rax, cr3
    mov cr3, rax

    ret
init_pat_end:

.text_end:
