.intel_syntax noprefix
.code16

.global ap_trampoline
.global ap_trampoline_end
.global ap_trampoline_cr3

# FIXME - is A20 guaranteed to be already set?

ap_trampoline:
    cli
    cld
    jmp 0:Lenter_protected_mode - ap_trampoline

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

.align 64
Lenter_protected_mode:
    # load GDT
    xor ax, ax
    mov ds, ax
    lgdt [Lgdt_table - ap_trampoline]

    # set PE flag in cr0
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x8:Lenter_long_mode - ap_trampoline

.code32
Lenter_long_mode:
    # set PAE bit in cr4
    mov eax, cr4
    or eax, 0x20
    mov cr4, eax

    # load CR3
    mov eax, [ap_trampoline_cr3 - ap_trampoline]
    mov cr3, eax

    # enable long mode
    mov ecx, 0xc0000080
    rdmsr
    or eax, 0x100
    wrmsr

    # enable paging
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    # load 64-bit GDT
    lgdt [Lgdt64_table - ap_trampoline]

    # jump to 64-bit code
    jmp 0x8:Llong_mode - ap_trampoline

.code64
Llong_mode:
    # FIXME
    jmp $

ap_trampoline_end:
