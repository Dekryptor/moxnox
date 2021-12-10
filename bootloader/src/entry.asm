global loader
global stack_ptr

extern main
extern go
extern pml4t
extern entry_point

MODULEALIGN equ 1<<0
MEMINFO equ 1<<1
FLAGS equ MODULEALIGN | MEMINFO
MAGIC equ 0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

section .mbheader
align 4
MultiBootHeader:
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .text

STACKSIZE equ 0x4000

loader:
    mov esp, stack+STACKSIZE
    push eax
    push ebx

    call main

    cli

hang:
    hlt
    jmp hang

go:
    ; set PAE bit
    mov eax, cr4
    or eax, 0x20
    mov cr4, eax

    ; set CR3
    mov eax, [pml4t]
    mov cr3, eax

    ; enable long mode
    mov ecx, 0xc0000080
    rdmsr
    or eax, 0x100
    wrmsr

    ; enable paging
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ; load GDT
    lgdt [gdt_ptr]

    jmp 0x08:start_kernel

bits 64
start_kernel:
    jmp [entry_point]

section .bss
align 4
stack:
    resb STACKSIZE
stack_ptr:

section .rodata
align 8
gdt:
     ; null GDT
    dq 0
    ; code GDT (64-bit)
    dw 0xffff ; LimitLow
    dw 0x0000 ; BaseLow
    db 0x00 ; BaseMid
    db 0x9a ; Type = 1a, Dpl = 0, Pres = 1
    db 0xaf ; LimitHi = f, Sys = 0, Long = 1, Default_Big = 0, Granularity = 1
    db 0x00 ; BaseHi
gdt_ptr:
    dw 15
    dd gdt
    dd 0
