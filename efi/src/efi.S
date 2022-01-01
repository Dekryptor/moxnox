.intel_syntax noprefix

.global start_kernel
start_kernel:
    # rcx is entry_point
    # rdx is pml4t
    # r8 is lb

    cli

    # change to virtual addresses
    mov cr3, rdx

    # FIXME - enable SSE etc. (check spec for what is set for us)

    mov rsp, 0xfffff90000003fe0 # KERNEL_STACK + (KERNEL_STACK_PAGES << PAGE_SHIFT) - 0x20 (0x20 for MS ABI)

    # FIXME - GDT?

    # enable NX bit and System Call Extensions
    mov rbx, rcx
    mov ecx, 0xc0000080
    rdmsr
    or eax, 0x801
    wrmsr

    # jump to entry point
    mov rcx, r8
    jmp rbx
