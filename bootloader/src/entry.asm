global loader
global stack_ptr

extern main
extern go
extern pml4t
extern entry_point
extern enable_sse
extern lb
extern KERNEL_STACK
extern STACK_SIZE

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

enable_sse:
    ; clear EM flag on CR0 and set MP flag
    mov eax, cr0
    and eax, 0xfffffffb
    or eax, 0x2
    mov cr0, eax

    ; set OSFXSR and OSXMMEXCPT flags on CR4
    mov eax, cr4
    or ax, 0x600
    mov cr4, eax
    ret

go:
    ; set PAE bit
    mov eax, cr4
    or eax, 0x20
    mov cr4, eax

    ; set CR3
    mov eax, [pml4t]
    mov cr3, eax

    ; enable long mode and NX bit
    mov ecx, 0xc0000080
    rdmsr
    or eax, 0x900
    wrmsr

    ; enable paging and WP bit
    mov eax, cr0
    or eax, 0x80010000
    mov cr0, eax

    ; load GDT
    lgdt [gdt_ptr]

    jmp 0x08:start_kernel

bits 64
start_kernel:
    mov ecx, lb ; clears high bits of rcx
    mov rsp, [rel KERNEL_STACK]
    add rsp, [rel STACK_SIZE]
    sub rsp, 0x20 ; MS calling convention needs 32 bytes above stack
    call [entry_point]

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
    ; data GDT
    dw 0xffff ; LimitLow
    dw 0x0000 ; BaseLow
    db 0x00 ; BaseMid
    db 0x92 ; Type = 2, Dpl = 0, Pres = 1
    db 0xaf ; LimitHi = f, Sys = 0, Long = 1, Default_Big = 0, Granularity = 1
    db 0x00 ; BaseHi
gdt_ptr:
    dw 23
    dd gdt
    dd 0
