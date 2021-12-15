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
    # FIXME - segments, EFLAGS, debug registers, FXSAVE
.endm

.global timer_isr # FIXME - needs own stack
timer_isr:
    save_context
    xor rax, rax
    mov DWORD PTR [0xfffffab0000000b0], eax # EOI
    mov rax, [rip+thread_context]
    iretq
