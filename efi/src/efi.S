.intel_syntax noprefix

.global start_kernel
start_kernel:
    # rcx is entry_point
    # rdx is pml4t

    cli

    # change to virtual addresses
    mov cr3, rdx

    # FIXME - set NX bit, enable SSE, etc. (check spec for what is set for us)
    # FIXME - stack
    # FIXME - GDT?
    # FIXME - jump to entry point

    # FIXME - pass through loader block
    jmp rcx
