.intel_syntax noprefix
.code16

.global ap_trampoline
.global ap_trampoline_end
.global ap_trampoline_cr3
.global ap_trampoline_start_func

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
ap_trampoline_start_func: # gets filled in by init_smp
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
    # clear EM flag on CR0 and set MP flag
    mov rax, cr0
    and rax, 0xfffffffffffffffb
    or rax, 0x2
    mov cr0, rax

    # set OSFXSR and OSXMMEXCPT flags on CR4
    mov rax, cr4
    or ax, 0x600
    mov cr4, rax

    # get CPU no.
    mov rax, 1
    cpuid
    shr ebx, 24 # ebx = ap_id

    # set stack (KERNEL_STACK | ((ap_id * KERNEL_STACK_PAGES) << PAGE_SHIFT))
    and ebx, 0xff
    movabs rsp, 0xfffff90000000000
    shl rbx, 14 # ap_id *= 0x4000 (KERNEL_STACKPAGES << PAGE_SHIFT)
    or rsp, rbx

    # add (KERNEL_STACK_PAGES << PAGE_SHIFT) - 0x20 (0x20 bytes for MS ABI)
    add rsp, 0x3fe0

    mov rax, [ap_trampoline_start_func - ap_trampoline]
    jmp rax

ap_trampoline_end:
