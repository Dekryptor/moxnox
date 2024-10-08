.intel_syntax noprefix

#include "context.S"

.global init_serial_port
init_serial_port:
    # cx = port

    # disable interrupts
    mov dx, cx
    inc dx
    xor al, al
    out dx, al

    # enable DLAB
    mov dx, cx
    add dx, 3
    mov al, 0x80
    out dx, al

    # set divisor to 3 (38400 baud)
    mov dx, cx
    mov al, 0x03
    out dx, al
    inc dx
    xor al, al
    out dx, al

    # 8 bits, no parity, 1 stop bit
    mov dx, cx
    add dx, 3
    mov al, 0x03
    out dx, al

    # enable FIFOs, clear them, and set interrupt trigger level to 14 bytes
    mov dx, cx
    add dx, 2
    mov al, 0xc7
    out dx, al

    # put in loopback mode
    mov dx, cx
    add dx, 4
    mov al, 0x10
    out dx, al

    # test
    mov dx, cx
    mov al, 0xa5
    out dx, al
    in al, dx

    cmp al, 0xa5
    je 0f
    xor rax, rax
    ret

0:
    # serial port okay, enable IRQs and put in normal mode
    mov dx, cx
    add dx, 4
    mov al, 0x0f
    out dx, al

    # send IRQ when transmit buffer is empty or data available
    mov dx, cx
    inc dx
    mov al, 0x03
    out dx, al

    mov rax, 1

    ret

.global serial_isr
serial_isr:
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

    mov dx, 0x3fa # interrupt identification register, COM1
    in al, dx
    mov cl, al
    sub rsp, 0x20
    call serial_isr_func
    add rsp, 0x20

    xor rax, rax
    mov DWORD PTR [APIC_BASE+0xb0], eax # EOI

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
