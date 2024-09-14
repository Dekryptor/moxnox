.intel_syntax noprefix
.code16

.section .rm_text
.globl enter_protected_mode
enter_protected_mode:
    cli

    # change GDT pointer to flat address
    mov eax, ds
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
    mov ebx, ds
    mov ds, ax

    mov eax, ebx
    shl eax, 4
    or eax, DWORD PTR [ptr_e820_entries]
    mov DWORD PTR [ptr_e820_entries], eax

    mov eax, ebx
    shl eax, 4
    or eax, DWORD PTR [ptr_e820_count]
    mov DWORD PTR [ptr_e820_count], eax

    mov ax, 0x10
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    and esp, ~3

    mov eax, OFFSET protected_mode_main
    jmp eax

.code64
.section .text
.globl long_mode_start
long_mode_start:
    jmp long_mode_start

.section .data
.globl ptr_e820_entries
ptr_e820_entries:
.long e820_entries
.globl ptr_e820_count
ptr_e820_count:
.long e820_count
