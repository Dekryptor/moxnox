.intel_syntax noprefix

#include "context.S"

.global keyboard_isr
keyboard_isr:
    sub rsp, 0x58

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

    in al, 0x60
    mov cl, al
    call keyboard_isr_func

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
