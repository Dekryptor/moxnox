.intel_syntax noprefix

.global keyboard_isr
keyboard_isr:
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11

    in al, 0x60
    mov cl, al
    call keyboard_isr_func

    xor rax, rax
    mov DWORD PTR [0xfffffab0000000b0], eax # EOI

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    iretq
