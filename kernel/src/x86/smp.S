.intel_syntax noprefix
.code16

.global ap_trampoline
.global ap_trampoline_end
.global ap_trampoline_cr3
.global ap_trampoline_start_func
.global ap_trampoline_rsp

# FIXME - is A20 guaranteed to be already set?

.equ ENTER_PROTECTED_MODE, Lenter_protected_mode - ap_trampoline
.equ ENTER_LONG_MODE_ADDRESS, Lenter_long_mode - ap_trampoline
.equ GDT_TABLE, Lgdt_table - ap_trampoline
.equ TRAMPOLINE_CR3, ap_trampoline_cr3 - ap_trampoline
.equ GDT64_TABLE, Lgdt64_table - ap_trampoline
.equ LONG_MODE_ADDRESS, Llong_mode - ap_trampoline
.equ TRAMPOLINE_ADDRESS, ap_trampoline_start_func - ap_trampoline
.equ RSP_ADDRESS, ap_trampoline_rsp - ap_trampoline

ap_trampoline:
    cli
    cld
    jmp 0x0, OFFSET ENTER_PROTECTED_MODE

.align 8
Lgdt:
    # null selector
    .long 0
    .long 0
    # code
    .long 0x0000ffff
    .long 0x00cf9a00
    # data
    .long 0x0000ffff
    .long 0x008f9200
Lgdt_table:
    .word Lgdt_table - Lgdt - 1
    .long Lgdt - ap_trampoline

.align 8
Lgdt64:
    # null selector
    .long 0
    .long 0
    # code
    .long 0x0000ffff
    .long 0x00af9a00
    # data
    .long 0x0000ffff
    .long 0x00af9200
Lgdt64_table:
    .word Lgdt64_table - Lgdt64 - 1
    .long Lgdt64 - ap_trampoline

ap_trampoline_cr3: # gets filled in by init_smp
    .long 0
ap_trampoline_start_func: # gets filled in by init_smp
    .quad 0
ap_trampoline_rsp: # gets filled in by init_smp
    .quad 0

.align 64
Lenter_protected_mode:
    # load GDT
    xor ax, ax
    mov ds, ax
    lgdt [GDT_TABLE]

    # set PE flag in cr0
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x8, OFFSET ENTER_LONG_MODE_ADDRESS

.code32
Lenter_long_mode:
    # set PAE bit in cr4
    mov eax, cr4
    or eax, 0x20
    mov cr4, eax

    # load CR3
    mov eax, [TRAMPOLINE_CR3]
    mov cr3, eax

    # enable long mode, NX bit, and System Call Extensions
    mov ecx, 0xc0000080
    rdmsr
    or eax, 0x901
    wrmsr

    # enable paging and WP bit
    mov eax, cr0
    or eax, 0x80010000
    mov cr0, eax

    # load 64-bit GDT
    lgdt [GDT64_TABLE]

    # jump to 64-bit code
    jmp 0x8, OFFSET LONG_MODE_ADDRESS

.code64
Llong_mode:
    # clear EM flag on CR0 and set MP flag
    mov rax, cr0
    and rax, 0xfffffffffffffffb
    or rax, 0x2
    mov cr0, rax

    # set OSFXSR and OSXMMEXCPT flags on CR4
    mov rax, cr4
    or ax, 0x600
    mov cr4, rax

    mov rsp, [RSP_ADDRESS]
    mov rax, [TRAMPOLINE_ADDRESS]
    jmp rax

ap_trampoline_end:
