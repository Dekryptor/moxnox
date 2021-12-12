.intel_syntax noprefix

.global dummy_isr
dummy_isr:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    call dummy_isr_func

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq

.global page_fault_isr
page_fault_isr:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    mov rdi, cr2
    mov rsi, [rsp+0x48]
    call page_fault

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax

    add rsp, 0x8 # pop error code

    iretq

.global double_fault_isr
double_fault_isr:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    call double_fault

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax

    add rsp, 0x8 # pop error code

    iretq

.global general_protection_fault_isr
general_protection_fault_isr:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    mov rdi, [rsp+0x48]
    call general_protection_fault

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax

    add rsp, 0x8 # pop error code

    iretq
