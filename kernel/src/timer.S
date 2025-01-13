.intel_syntax noprefix

#include "context.S"
#include "tick_rate.h"

.equ IA32_KERNEL_GS_BASE, 0xc0000102

.global timer_isr_ap
timer_isr_ap:
    cmp QWORD PTR [rsp+0x8], 0x8
    je Ltimer_isr_threads
    swapgs
    load_kernel_cr3

Ltimer_isr_threads:
    cmp QWORD PTR gs:GS_CURRENT_THREAD, 0
    je Ltimer_isr_nop

    cmp QWORD PTR gs:GS_DISABLE_PREEMPT, 0
    jne Ltimer_isr_nop

    dec QWORD PTR gs:GS_TIME_SLICE_LEFT
    cmp QWORD PTR gs:GS_TIME_SLICE_LEFT, 0
    jne Ltimer_isr_nop

    save_context
    xor rax, rax
    mov DWORD PTR [APIC_BASE+APIC_EOI], eax
    call acquire_sync_lock
    mov rsp, gs:GS_SCHEDULER_STACK
    jmp scheduler

Ltimer_isr_nop:
    push rax
    xor rax, rax
    mov DWORD PTR [APIC_BASE+APIC_EOI], eax
    pop rax
    cmp QWORD PTR [rsp+0x8], 0x8
    je 1f
    load_process_cr3
    swapgs
1:
    iretq

.global timer_isr
timer_isr:
    cmp QWORD PTR [rsp+0x8], 0x8
    je 0f
    swapgs
    load_kernel_cr3
0:
    add QWORD PTR [rip+system_time], SYSTEM_TIME_RESOLUTION / TICK_RATE

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
    jmp Ltimer_isr_threads

.global scheduler_restore
scheduler_restore:
    movzx rax, WORD PTR gs:GS_CONTEXT + CONTEXT_SS
    push rax
    push gs:GS_CONTEXT + CONTEXT_RSP
    mov eax, gs:GS_CONTEXT + CONTEXT_EFLAGS
    push rax
    movzx rax, WORD PTR gs:GS_CONTEXT + CONTEXT_CS
    push rax
    push gs:GS_CONTEXT + CONTEXT_RIP
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
    mov ds, gs:GS_CONTEXT + CONTEXT_DS
    mov es, gs:GS_CONTEXT + CONTEXT_ES
    mov fs, gs:GS_CONTEXT + CONTEXT_FS
    # setting gs nukes gsbase
    # FIXME - debug registers
    fxrstor gs:GS_CONTEXT + CONTEXT_FXSAVE

    push rax

    # set GS base to TEB
    push rcx
    push rdx
    mov rax, gs:GS_CURRENT_THREAD
    mov rax, [rax+THREAD_TEB]
    mov rdx, rax
    shr rdx, 32
    and eax, 0xffffffff
    mov ecx, IA32_KERNEL_GS_BASE
    wrmsr
    pop rdx
    pop rcx

    # change cr3 and swap GS, if currently in user mode
    mov rax, gs:GS_CURRENT_THREAD
    cmp BYTE PTR [rax + THREAD_KERNEL_MODE], 0
    jne 0f
    mov rax, [rax+THREAD_PROC]
    mov rax, [rax+PROCESS_CR3]
    mov cr3, rax
    swapgs
0:
    pop rax

    iretq

.global yield
yield:
    pushfq
    or QWORD PTR [rsp], 0x200 # set interrupt enable flag in stored RFLAGS
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
    mov rax, [rsp] # RFLAGS
    mov gs:GS_CONTEXT + CONTEXT_EFLAGS, eax
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_CS, cs
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_DS, ds
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_ES, es
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_FS, fs
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_GS, gs
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_SS, ss
    fxsave gs:GS_CONTEXT + CONTEXT_FXSAVE
    # FIXME - debug registers

    mov rsp, gs:GS_SCHEDULER_STACK
    jmp scheduler

.global acquire_sync_lock
acquire_sync_lock: # clobbers rax
    mov rax, 1
0:
    xchg [rip+sync_lock_val], rax
    test rax, rax
    jnz 0b
#ifdef DEBUG
    mov eax, gs:GS_CPU_NO
    mov DWORD PTR [rip+sync_lock_cpu], eax
#endif
    ret

.global release_sync_lock
release_sync_lock: # clobbers rax
    xor rax, rax
    xchg [rip+sync_lock_val], rax
#ifdef DEBUG
    mov DWORD PTR [rip+sync_lock_cpu], 0xffffffff
#endif
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
