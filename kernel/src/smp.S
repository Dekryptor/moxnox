.intel_syntax noprefix
.code16

.global ap_trampoline
.global ap_trampoline_end

# FIXME - is A20 guaranteed to be already set?

ap_trampoline:
    cli
    cld
    jmp 0:Lprotected_mode - ap_trampoline

.align 16
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

.align 64
Lprotected_mode:
    # load GDT
    xor ax, ax
    mov ds, ax
    lgdt [Lgdt_table - ap_trampoline]

    # set PE flag in cr0
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp $

ap_trampoline_end:
