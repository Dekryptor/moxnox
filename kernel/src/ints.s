.intel_syntax noprefix

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
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11

    mov rcx, [rsp+0x38]
    call general_protection_fault

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax

    add rsp, 0x8 # pop error code

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
