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

    jmp 0x08:1f

.code32
1:
    jmp 1b # FIXME
    # jmp protected_mode_main
