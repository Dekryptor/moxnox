.intel_syntax noprefix

#include "context.S"

.global keyboard_isr
keyboard_isr:
    cmp QWORD PTR [rsp+0x8], 0x8
    je 0f
    swapgs
0:
    load_kernel_cr3
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11

    in al, 0x60
    mov cl, al
    sub rsp, 0x20
    call keyboard_isr_func
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

    cmp QWORD PTR [rsp+0x8], 0x8
    je 1f
    swapgs
1:
    iretq
