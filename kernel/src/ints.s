.intel_syntax noprefix

.macro save_context_when_error_code # clobbers rax
    mov [rip+thread_context], rax
    mov [rip+thread_context+0x08], rcx
    mov [rip+thread_context+0x10], rdx
    mov [rip+thread_context+0x18], rbx
    mov rax, [rsp+0x20]
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
    mov rax, [rsp+0x8]
    mov [rip+thread_context+0x80], rax # old rip
    mov rax, [rsp+0x18]
    mov [rip+thread_context+0x88], rax # old RFLAGS
    mov rax, [rsp+0x10]
    mov [rip+thread_context+0x90], rax # old cs
    mov rax, [rsp+0x28]
    mov [rip+thread_context+0x98], rax # old ss
    # FIXME - other segments, debug registers, FXSAVE
.endm

.global dummy_isr
dummy_isr:
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11

    call dummy_isr_func

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    iretq

.global page_fault_isr
page_fault_isr:
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11

    mov rcx, cr2
    mov rdx, [rsp+0x38]
    call page_fault

    pop r11
    pop r10
    pop r9
    pop r8
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
    push r8
    push r9
    push r10
    push r11

    call double_fault

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax

    add rsp, 0x8 # pop error code

    iretq

.global general_protection_fault_isr
general_protection_fault_isr:
    save_context_when_error_code
    pop rcx
    call general_protection_fault
    iretq

.global keyboard_isr
keyboard_isr:
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11

    call keyboard_isr_func

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    iretq

.global spurious_isr
spurious_isr:
    iretq
