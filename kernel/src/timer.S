.intel_syntax noprefix

#include "context.S"

.global timer_isr # FIXME - needs own stack
timer_isr:
    cmp QWORD PTR gs:0x8, 0 # current thread
    je timer_isr_nop

    dec QWORD PTR gs:0x10
    cmp QWORD PTR gs:0x10, 0
    jne timer_isr_nop

    save_context
    xor rax, rax
    mov DWORD PTR [0xfffffab0000000b0], eax # EOI
    jmp scheduler

timer_isr_nop:
    push rax
    xor rax, rax
    mov DWORD PTR [0xfffffab0000000b0], eax # EOI
    pop rax
    iretq

.global scheduler_restore
scheduler_restore:
    mov rax, gs:0x18
    mov rcx, gs:0x20
    mov rdx, gs:0x28
    mov rbx, gs:0x30
    mov rbp, gs:0x40
    mov rsi, gs:0x48
    mov rdi, gs:0x50
    mov r8, gs:0x58
    mov r9, gs:0x60
    mov r10, gs:0x68
    mov r11, gs:0x70
    mov r12, gs:0x78
    mov r13, gs:0x80
    mov r14, gs:0x88
    mov r15, gs:0x90
    # FIXME - other segments, debug registers, FXSAVE
    push gs:0xb0 # ss
    push gs:0x38 # rsp
    push gs:0xa0 # RFLAGS
    push gs:0xa8 # cs
    push gs:0x98 # rip
    iretq

.global yield
yield:
    pushfq
    or QWORD PTR [rsp], 0x200 # set interrupt enable flag in stored RFLAGS
    cli
    mov gs:0x18, rax
    mov gs:0x20, rcx
    mov gs:0x28, rdx
    mov gs:0x30, rbx
    lea rax, [rsp+0x10]
    mov gs:0x38, rax # rsp
    mov gs:0x40, rbp
    mov gs:0x48, rsi
    mov gs:0x50, rdi
    mov gs:0x58, r8
    mov gs:0x60, r9
    mov gs:0x68, r10
    mov gs:0x70, r11
    mov gs:0x78, r12
    mov gs:0x80, r13
    mov gs:0x88, r14
    mov gs:0x90, r15
    mov rax, [rsp+0x8]
    mov gs:0x98, rax # rip of instruction after call
    pop gs:0xa0 # RFLAGS
    mov gs:0xa8, cs
    mov gs:0xb0, ss
    # FIXME - other segments, debug registers, FXSAVE
    jmp scheduler

.global acquire_sync_lock
acquire_sync_lock:
    mov rax, 1
    xchg [rip+sync_lock], rax
    test rax, rax
    jnz acquire_sync_lock
    ret

.global release_sync_lock
release_sync_lock:
    xor rax, rax
    xchg [rip+sync_lock], rax
    ret

.global pit_delay
pit_delay:
    mov al, 0x30
    out 0x43, al # initialize channel 0 of PIT
    mov ax, cx
    out 0x40, al # push lower byte of param
    shr ax, 8
    out 0x40, al # push higher byte of param
0:
    mov al, 0xe2
    out 0x43, al # check PIT status
    in al, 0x40
    test al, 0x80
    je 0b # loop again if not yet signalled
    ret

.global get_cpu_block
get_cpu_block:
    mov rax, gs:0
    ret
