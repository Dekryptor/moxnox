.intel_syntax noprefix
.code16

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

    # long jump
    .byte 0x66
    .byte 0xea
    .long OFFSET 1f + 0x10000 # FIXME
    .short 0x8

.code32
1:
    jmp protected_mode_main
