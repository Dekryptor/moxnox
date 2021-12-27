.intel_syntax noprefix

#include "context.S"

.global dummy_isr
dummy_isr:
    load_kernel_cr3
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11

    sub rsp, 0x20
    call dummy_isr_func
    add rsp, 0x20

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    load_process_cr3
    iretq

.global page_fault_isr
page_fault_isr: # keeps process' cr3
    save_context_when_error_code
    mov rcx, cr2
    pop rdx
    sub rsp, 0x20
    call page_fault
    add rsp, 0x20
    iretq

.global double_fault_isr
double_fault_isr:
    load_kernel_cr3
    save_context_when_error_code
    pop rcx
    sub rsp, 0x20
    call double_fault
    add rsp, 0x20
    load_process_cr3
    iretq

.global general_protection_fault_isr
general_protection_fault_isr:
    load_kernel_cr3
    save_context_when_error_code
    pop rcx
    sub rsp, 0x20
    call general_protection_fault
    add rsp, 0x20
    load_process_cr3
    iretq

.global invalid_opcode_isr
invalid_opcode_isr:
    load_kernel_cr3
    save_context
    sub rsp, 0x20
    call invalid_opcode
    add rsp, 0x20
    load_process_cr3
    iretq

.global spurious_isr
spurious_isr:
    iretq
