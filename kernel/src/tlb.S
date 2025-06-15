.intel_syntax noprefix

#include "context_offsets.h"

.equ PAGE_SIZE, 0x1000

.bss

.global tlb_va
tlb_va:
.space 8

.global tlb_num_pages
tlb_num_pages:
.space 8

.global tlb_cpus_left
tlb_cpus_left:
.space 4

.text

.global invalidate_tlb_isr
invalidate_tlb_isr:
    push rax
    push rcx

    mov rax, QWORD PTR [rip+tlb_va]
    mov rcx, QWORD PTR [rip+tlb_num_pages]
0:
    invlpg [rax]
    cmp rcx, 0
    je 1f
    add rax, PAGE_SIZE
    dec rcx
    jmp 0b

1:
    lock dec DWORD PTR [rip+tlb_cpus_left]

    xor rax, rax
    mov DWORD PTR [APIC_BASE+APIC_EOI], eax

    pop rcx
    pop rax
    iretq
invalidate_tlb_isr_end:

