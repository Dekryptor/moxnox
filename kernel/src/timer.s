.intel_syntax noprefix

.global timer_count
.global thread_context

.global pit_isr
pit_isr:
    cmp DWORD PTR [rip+timer_count], 0
    je pit_isr_end
    dec QWORD PTR [rip+timer_count]
pit_isr_end:
    push rax
    xor rax, rax
    mov DWORD PTR [0xfffffab0000000b0], eax # EOI
    pop rax
    iretq

.macro save_context # clobbers rax
    mov [rip+thread_context], rax
    mov [rip+thread_context+0x08], rcx
    mov [rip+thread_context+0x10], rdx
    mov [rip+thread_context+0x18], rbx
    mov rax, [rsp+0x18]
    mov [rip+thread_context+0x20], rax # old rsp
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
    mov rax, [rsp]
    mov [rip+thread_context+0x80], rax # old rip
    pushfq
    pop [rip+thread_context+0x88] # RFLAGS
    mov [rip+thread_context+0x90], cs
    mov [rip+thread_context+0x98], ss
    # FIXME - other segments, debug registers, FXSAVE
.endm

.global timer_isr # FIXME - needs own stack
timer_isr:
    save_context
    xor rax, rax
    mov DWORD PTR [0xfffffab0000000b0], eax # EOI
    mov rax, [rip+thread_context]
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
