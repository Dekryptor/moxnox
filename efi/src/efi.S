.intel_syntax noprefix

.global start_kernel
start_kernel:
    # rcx is entry_point
    # rdx is pml4t
    # r8 is lb

    cli

    # change to virtual addresses
    mov cr3, rdx

    mov rsp, 0xfffff90000003fe0 # KERNEL_STACK + (KERNEL_STACK_PAGES << PAGE_SHIFT) - 0x20 (0x20 for MS ABI)

    # FIXME - GDT?

    # set MP and WP bits on CR0
    mov rax, cr0
    or rax, 0x10002
    mov cr0, rax

    # set OSFXSR and OSXMMEXCPT flags on CR4
    mov rax, cr4
    or rax, 0x600
    mov cr4, rax

    # enable NX bit and System Call Extensions
    mov rbx, rcx
    mov ecx, 0xc0000080
    rdmsr
    or eax, 0x801
    wrmsr

    # jump to entry point
    mov rcx, r8
    jmp rbx
