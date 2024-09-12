.intel_syntax noprefix
.code16

.section .rm_text
.globl enter_protected_mode
enter_protected_mode:
    cli

    # change GDT pointer to flat address
    mov ax, ds
    shl eax, 4
    or eax, DWORD PTR [gdt_descriptor + 2]
    mov DWORD PTR [gdt_descriptor + 2], eax

    lgdt [gdt_descriptor]

    mov eax, cr0
    or al, 1
    mov cr0, eax

    mov ax, cs
    shl eax, 4
    or eax, OFFSET 1f

    push 0x8
    push eax

    # 32-bit retf
    .byte 0x66
    .byte 0xcb

.code32
1:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    mov eax, OFFSET protected_mode_main
    jmp eax
