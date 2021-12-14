.intel_syntax noprefix

.global timer_count

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
