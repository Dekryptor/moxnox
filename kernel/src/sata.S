.intel_syntax noprefix

#include "context.S"

.global sata_isr
sata_isr:
    load_kernel_cr3
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11

    sub rsp, 0x20
    call sata_isr_func
    add rsp, 0x20

    xor rax, rax
    mov DWORD PTR [0xfffffab0000000b0], eax # EOI

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    load_process_cr3
    iretq
