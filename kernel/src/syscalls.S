.intel_syntax noprefix

.global init_syscalls
init_syscalls:
    mov ecx, 0xc0000082 # LSTAR
    lea rax, [rip+syscall_handler]
    mov rdx, rax
    shr rdx, 32
    wrmsr

    mov ecx, 0xc0000081 # STAR
    xor eax, eax
    mov edx, 0x002b0008 # ((user CS - 16) << 16) | kernel CS
    wrmsr

    ret

.global syscall_handler
syscall_handler:
    # FIXME - swapgs
    # FIXME - change to kernel cr3
    # FIXME - change to kernel stack

    # FIXME - call handler based on rax

    # FIXME - swapgs
    # FIXME - change back to user stack
    # FIXME - change back to process cr3

    sysretq
