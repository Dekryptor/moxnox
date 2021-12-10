global loader
global stack_ptr

extern main
extern enable_paging
extern pml4t

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

enable_paging:
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

    ret

section .bss
align 4
stack:
    resb STACKSIZE
stack_ptr:
