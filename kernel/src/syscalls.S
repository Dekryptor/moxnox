.intel_syntax noprefix

#include "context_offsets.h"

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

    mov ecx, 0xc0000084 # SFMASK
    xor edx, edx
    mov eax, 0x200 # disable interrupts when syscall_handler called
    wrmsr

    ret

.global syscall_handler
syscall_handler:
    swapgs

    # save thread context
    mov gs:GS_CONTEXT + CONTEXT_RAX, rax
    # rcx has been clobbered
    mov gs:GS_CONTEXT + CONTEXT_RDX, rdx
    mov gs:GS_CONTEXT + CONTEXT_RBX, rbx
    mov gs:GS_CONTEXT + CONTEXT_RSP, rsp
    mov gs:GS_CONTEXT + CONTEXT_RBP, rbp
    mov gs:GS_CONTEXT + CONTEXT_RSI, rsi
    mov gs:GS_CONTEXT + CONTEXT_RDI, rdi
    mov gs:GS_CONTEXT + CONTEXT_R8, r8
    mov gs:GS_CONTEXT + CONTEXT_R9, r9
    mov gs:GS_CONTEXT + CONTEXT_R10, r10
    # r11 has been clobbered
    mov gs:GS_CONTEXT + CONTEXT_R12, r12
    mov gs:GS_CONTEXT + CONTEXT_R13, r13
    mov gs:GS_CONTEXT + CONTEXT_R14, r14
    mov gs:GS_CONTEXT + CONTEXT_R15, r15
    mov gs:GS_CONTEXT + CONTEXT_RIP, rcx
    mov gs:GS_CONTEXT + CONTEXT_RFLAGS, r11
    # cs and ss have been clobbered
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_DS, ds
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_ES, es
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_FS, fs
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_GS, gs
    # FIXME - debug registers
    fxsave gs:GS_CONTEXT + CONTEXT_FXSAVE

    # change to kernel stack
    mov rax, gs:GS_CURRENT_THREAD
    mov rsp, [rax + THREAD_SYSCALL_STACK]

    # load kernel cr3
    mov rax, gs:GS_KERNEL_CR3
    mov cr3, rax
    pop rax

    # load kernel ds segment
    mov ax, 0x10
    mov ds, ax

    call syscall

    # load user ds segment
    push rax
    mov ax, 0x33
    mov ds, ax

    # load process cr3
    mov rax, gs:GS_CURRENT_THREAD
    mov rax, [rax+THREAD_PROC]
    mov rax, [rax+PROCESS_CR3]
    mov cr3, rax
    pop rax

    # change back to user stack
    mov rsp, gs:GS_CONTEXT + CONTEXT_RSP

    mov rcx, gs:GS_CONTEXT + CONTEXT_RIP
    mov r11, gs:GS_CONTEXT + CONTEXT_RFLAGS
    swapgs

    sysretq
