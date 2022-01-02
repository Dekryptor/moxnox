.intel_syntax noprefix

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

    # send IRQ when transmit buffer is empty
    mov dx, cx
    inc dx
    mov al, 0x02
    out dx, al

    mov rax, 1

    ret

.global serial_isr
serial_isr:
    # FIXME

    push rax
    xor rax, rax
    mov DWORD PTR [0xfffffab0000000b0], eax # EOI
    pop rax

    iretq
