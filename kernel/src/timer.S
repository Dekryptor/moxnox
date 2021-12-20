.intel_syntax noprefix

#include "context.S"

.global timer_isr # FIXME - needs own stack
timer_isr:
    # FIXME - different ISR for CPUs not handling timers?
    cmp BYTE PTR gs:GS_HANDLE_TIMERS, 0 # only handle timers on one CPU
    je Ltimer_isr_threads
    cmp QWORD PTR gs:GS_PREEMPT_BLOCK, 0 # ignore timers if in spinlock
    jne Ltimer_isr_threads

    push rax
    push rcx
    push rdx
    call acquire_sync_lock

    mov rax, [rip+active_timer_list] # set rax to active_timer_list.next
    lea rcx, [rip+active_timer_list] # set rcx to &active_timer_list

Ltimer_isr_loop_start:
    cmp rax, rcx
    je Ltimer_isr_loop_end

    mov rdx, [rax] # set rdx to le->next
    sub rax, TIMER_LE

    dec QWORD PTR [rax+TIMER_TICKS_LEFT] # reduce ticks_left
    cmp QWORD PTR [rax+TIMER_TICKS_LEFT], 0
    jne Ltimer_isr_not_expired

    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    mov rcx, rax
    sub rsp, 0x20
    call timer_expired
    add rsp, 0x20
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax

Ltimer_isr_not_expired:
    mov rax, rdx
    jmp Ltimer_isr_loop_start

Ltimer_isr_loop_end:
    call release_sync_lock
    pop rdx
    pop rcx
    pop rax

Ltimer_isr_threads:
    cmp QWORD PTR gs:GS_CURRENT_THREAD, 0
    je Ltimer_isr_nop

    cmp QWORD PTR gs:GS_PREEMPT_BLOCK, 0
    jne Ltimer_isr_blocked

    dec QWORD PTR gs:GS_TIME_SLICE_LEFT
    cmp QWORD PTR gs:GS_TIME_SLICE_LEFT, 0
    jne Ltimer_isr_nop

    save_context
    xor rax, rax
    mov DWORD PTR [0xfffffab0000000b0], eax # EOI
    jmp scheduler

Ltimer_isr_nop:
    push rax
    xor rax, rax
    mov DWORD PTR [0xfffffab0000000b0], eax # EOI
    pop rax
    iretq

Ltimer_isr_blocked:
    cmp QWORD PTR gs:GS_TIME_SLICE_LEFT, 0
    je Ltimer_isr_nop
    dec QWORD PTR gs:GS_TIME_SLICE_LEFT
    jmp Ltimer_isr_nop

.global scheduler_restore
scheduler_restore:
    mov rax, gs:GS_CONTEXT + CONTEXT_RAX
    mov rcx, gs:GS_CONTEXT + CONTEXT_RCX
    mov rdx, gs:GS_CONTEXT + CONTEXT_RDX
    mov rbx, gs:GS_CONTEXT + CONTEXT_RBX
    mov rbp, gs:GS_CONTEXT + CONTEXT_RBP
    mov rsi, gs:GS_CONTEXT + CONTEXT_RSI
    mov rdi, gs:GS_CONTEXT + CONTEXT_RDI
    mov r8, gs:GS_CONTEXT + CONTEXT_R8
    mov r9, gs:GS_CONTEXT + CONTEXT_R9
    mov r10, gs:GS_CONTEXT + CONTEXT_R10
    mov r11, gs:GS_CONTEXT + CONTEXT_R11
    mov r12, gs:GS_CONTEXT + CONTEXT_R12
    mov r13, gs:GS_CONTEXT + CONTEXT_R13
    mov r14, gs:GS_CONTEXT + CONTEXT_R14
    mov r15, gs:GS_CONTEXT + CONTEXT_R15
    # FIXME - other segments, debug registers
    push gs:GS_CONTEXT + CONTEXT_SS # ss
    push gs:GS_CONTEXT + CONTEXT_RSP # rsp
    push gs:GS_CONTEXT + CONTEXT_RFLAGS # RFLAGS
    push gs:GS_CONTEXT + CONTEXT_CS # cs
    push gs:GS_CONTEXT + CONTEXT_RIP # rip
    fxrstor gs:GS_CONTEXT + CONTEXT_FXSAVE
    iretq

.global yield
yield:
    pushfq
    or QWORD PTR [rsp], 0x200 # set interrupt enable flag in stored RFLAGS
    cli
    mov gs:GS_CONTEXT + CONTEXT_RAX, rax
    mov gs:GS_CONTEXT + CONTEXT_RCX, rcx
    mov gs:GS_CONTEXT + CONTEXT_RDX, rdx
    mov gs:GS_CONTEXT + CONTEXT_RBX, rbx
    lea rax, [rsp+0x10]
    mov gs:GS_CONTEXT + CONTEXT_RSP, rax # rsp
    mov gs:GS_CONTEXT + CONTEXT_RBP, rbp
    mov gs:GS_CONTEXT + CONTEXT_RSI, rsi
    mov gs:GS_CONTEXT + CONTEXT_RDI, rdi
    mov gs:GS_CONTEXT + CONTEXT_R8, r8
    mov gs:GS_CONTEXT + CONTEXT_R9, r9
    mov gs:GS_CONTEXT + CONTEXT_R10, r10
    mov gs:GS_CONTEXT + CONTEXT_R11, r11
    mov gs:GS_CONTEXT + CONTEXT_R12, r12
    mov gs:GS_CONTEXT + CONTEXT_R13, r13
    mov gs:GS_CONTEXT + CONTEXT_R14, r14
    mov gs:GS_CONTEXT + CONTEXT_R15, r15
    mov rax, [rsp+0x8]
    mov gs:GS_CONTEXT + CONTEXT_RIP, rax # rip of instruction after call
    pop gs:GS_CONTEXT + CONTEXT_RFLAGS # RFLAGS
    mov gs:GS_CONTEXT + CONTEXT_CS, cs
    mov gs:GS_CONTEXT + CONTEXT_SS, ss
    fxsave gs:GS_CONTEXT + CONTEXT_FXSAVE
    # FIXME - other segments, debug registers
    jmp scheduler

.global acquire_sync_lock
acquire_sync_lock: # clobbers rax
    inc QWORD PTR gs:GS_PREEMPT_BLOCK
Lacquire_sync_lock_loop:
    mov rax, 1
    xchg [rip+sync_lock], rax
    test rax, rax
    jnz Lacquire_sync_lock_loop
    ret

.global release_sync_lock
release_sync_lock: # clobbers rax
    xor rax, rax
    xchg [rip+sync_lock], rax
    dec QWORD PTR gs:GS_PREEMPT_BLOCK
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
