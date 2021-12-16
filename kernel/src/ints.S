.intel_syntax noprefix

#include "context.S"

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
    save_context_when_error_code
    mov rcx, cr2
    pop rdx
    call page_fault
    iretq

.global double_fault_isr
double_fault_isr:
    save_context_when_error_code
    pop rcx
    call double_fault
    iretq

.global general_protection_fault_isr
general_protection_fault_isr:
    save_context_when_error_code
    pop rcx
    call general_protection_fault
    iretq

.global invalid_opcode_isr
invalid_opcode_isr:
    save_context
    call invalid_opcode
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
