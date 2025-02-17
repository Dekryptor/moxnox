.intel_syntax noprefix

#include "context.S"
#include "dwarf.S"

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

.uleb128 2
.asciz "init_serial_port"
.quad init_serial_port
.quad init_serial_port_end - init_serial_port

.uleb128 2
.asciz "serial_isr"
.quad serial_isr
.quad serial_isr_end - serial_isr

.uleb128 0

.debug_info_end:

.text

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
init_serial_port_end:

.seh_proc serial_isr
.global serial_isr
serial_isr:
    .seh_pushframe
    sub rsp, 0x58
    .seh_stackalloc 0x58
    .seh_endprologue

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

    mov dx, 0x3fa # interrupt identification register, COM1
    in al, dx
    mov cl, al
    call serial_isr_func

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
    .seh_endproc
serial_isr_end:

.text_end:
