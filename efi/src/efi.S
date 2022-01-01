.intel_syntax noprefix

.global start_kernel
start_kernel:
    # rcx is entry_point
    # rdx is pml4t

    cli

    # change to virtual addresses
    mov cr3, rdx

    # FIXME - set NX bit, enable SSE, etc. (check spec for what is set for us)

    mov rsp, 0xfffff90000003fe0 # KERNEL_STACK + (KERNEL_STACK_PAGES << PAGE_SHIFT) - 0x20 (0x20 for MS ABI)

    # FIXME - GDT?

    # FIXME - pass through loader block

    # jump to entry point
    jmp rcx
