.intel_syntax noprefix

#include "context.S"

.global timer_isr # FIXME - needs own stack
timer_isr:
    cmp QWORD PTR [rip+current_thread], 0
    je timer_isr_nop

    dec QWORD PTR [rip+time_slice_left]
    cmp QWORD PTR [rip+time_slice_left], 0
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
    mov rax, [rip+thread_context]
    mov rcx, [rip+thread_context+0x08]
    mov rdx, [rip+thread_context+0x10]
    mov rbx, [rip+thread_context+0x18]
    mov rbp, [rip+thread_context+0x28]
    mov rsi, [rip+thread_context+0x30]
    mov rdi, [rip+thread_context+0x38]
    mov r8, [rip+thread_context+0x40]
    mov r9, [rip+thread_context+0x48]
    mov r10, [rip+thread_context+0x50]
    mov r11, [rip+thread_context+0x58]
    mov r12, [rip+thread_context+0x60]
    mov r13, [rip+thread_context+0x68]
    mov r14, [rip+thread_context+0x70]
    mov r15, [rip+thread_context+0x78]
    # FIXME - other segments, debug registers, FXSAVE
    push [rip+thread_context+0x98] # ss
    push [rip+thread_context+0x20] # rsp
    push [rip+thread_context+0x88] # RFLAGS
    push [rip+thread_context+0x90] # cs
    push [rip+thread_context+0x80] # rip
    iretq

.global yield
yield:
    pushfq
    or QWORD PTR [rsp], 0x200 # set interrupt enable flag in stored RFLAGS
    cli
    mov [rip+thread_context], rax
    mov [rip+thread_context+0x08], rcx
    mov [rip+thread_context+0x10], rdx
    mov [rip+thread_context+0x18], rbx
    lea rax, [rsp+0x10]
    mov [rip+thread_context+0x20], rax # rsp
    mov [rip+thread_context+0x28], rbp
    mov [rip+thread_context+0x30], rsi
    mov [rip+thread_context+0x38], rdi
    mov [rip+thread_context+0x40], r8
    mov [rip+thread_context+0x48], r9
    mov [rip+thread_context+0x50], r10
    mov [rip+thread_context+0x58], r11
    mov [rip+thread_context+0x60], r12
    mov [rip+thread_context+0x68], r13
    mov [rip+thread_context+0x70], r14
    mov [rip+thread_context+0x78], r15
    mov rax, [rsp+0x8]
    mov [rip+thread_context+0x80], rax # rip of instruction after call
    pop [rip+thread_context+0x88] # RFLAGS
    mov [rip+thread_context+0x90], cs
    mov [rip+thread_context+0x98], ss
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
