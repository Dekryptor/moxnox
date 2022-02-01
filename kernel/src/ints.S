.intel_syntax noprefix

#include "context.S"

.global dummy_isr
dummy_isr:
    cmp QWORD PTR [rsp+0x8], 0x8
    je 0f
    swapgs
0:
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

    cmp QWORD PTR [rsp+0x8], 0x8
    je 1f
    swapgs
1:
    iretq

.global page_fault_isr
page_fault_isr: # keeps process' cr3
    cmp QWORD PTR [rsp+0x10], 0x8
    je 0f
    swapgs
0:
    save_context_when_error_code
    mov rcx, cr2
    pop rdx
    sub rsp, 0x20
    call page_fault
    add rsp, 0x20

    mov r11, gs:GS_CONTEXT + CONTEXT_R11
    mov r10, gs:GS_CONTEXT + CONTEXT_R10
    mov r9, gs:GS_CONTEXT + CONTEXT_R9
    mov r8, gs:GS_CONTEXT + CONTEXT_R8
    mov rdx, gs:GS_CONTEXT + CONTEXT_RDX
    mov rcx, gs:GS_CONTEXT + CONTEXT_RCX
    mov rax, gs:GS_CONTEXT + CONTEXT_RIP
    mov QWORD PTR [rsp], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RSP
    mov QWORD PTR [rsp+0x18], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RAX

    cmp QWORD PTR [rsp+0x8], 0x8
    je 1f
    swapgs
1:
    iretq

.global double_fault_isr
double_fault_isr:
    cmp QWORD PTR [rsp+0x10], 0x8
    je 0f
    swapgs
0:
    load_kernel_cr3
    save_context_when_error_code
    pop rcx
    sub rsp, 0x20
    call double_fault
    add rsp, 0x20
    load_process_cr3

    cmp QWORD PTR [rsp+0x8], 0x8
    je 1f
    swapgs
1:
    iretq

.global general_protection_fault_isr
general_protection_fault_isr:
    cmp QWORD PTR [rsp+0x10], 0x8
    je 0f
    swapgs
0:
    load_kernel_cr3
    save_context_when_error_code
    pop rcx
    sub rsp, 0x20
    call general_protection_fault
    add rsp, 0x20
    load_process_cr3

    mov r11, gs:GS_CONTEXT + CONTEXT_R11
    mov r10, gs:GS_CONTEXT + CONTEXT_R10
    mov r9, gs:GS_CONTEXT + CONTEXT_R9
    mov r8, gs:GS_CONTEXT + CONTEXT_R8
    mov rdx, gs:GS_CONTEXT + CONTEXT_RDX
    mov rcx, gs:GS_CONTEXT + CONTEXT_RCX
    mov rax, gs:GS_CONTEXT + CONTEXT_RIP
    mov QWORD PTR [rsp], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RSP
    mov QWORD PTR [rsp+0x18], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RAX

    cmp QWORD PTR [rsp+0x8], 0x8
    je 1f
    swapgs
1:
    iretq

.global invalid_opcode_isr
invalid_opcode_isr:
    cmp QWORD PTR [rsp+0x8], 0x8
    je 0f
    swapgs
0:
    load_kernel_cr3
    save_context
    sub rsp, 0x20
    call invalid_opcode
    add rsp, 0x20
    load_process_cr3

    mov r11, gs:GS_CONTEXT + CONTEXT_R11
    mov r10, gs:GS_CONTEXT + CONTEXT_R10
    mov r9, gs:GS_CONTEXT + CONTEXT_R9
    mov r8, gs:GS_CONTEXT + CONTEXT_R8
    mov rdx, gs:GS_CONTEXT + CONTEXT_RDX
    mov rcx, gs:GS_CONTEXT + CONTEXT_RCX
    mov rax, gs:GS_CONTEXT + CONTEXT_RIP
    mov QWORD PTR [rsp], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RSP
    mov QWORD PTR [rsp+0x18], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RAX

    cmp QWORD PTR [rsp+0x8], 0x8
    je 1f
    swapgs
1:
    iretq

.global spurious_isr
spurious_isr:
    push rax
    xor rax, rax
    mov DWORD PTR [0xfffffab0000000b0], eax # EOI
    pop rax
    iretq

.global halt_isr
halt_isr:
    hlt
    jmp halt_isr

.global divide_by_zero_isr
divide_by_zero_isr:
    cmp QWORD PTR [rsp+0x8], 0x8
    je 0f
    swapgs
0:
    load_kernel_cr3
    save_context
    sub rsp, 0x20
    call divide_by_zero
    add rsp, 0x20
    load_process_cr3

    mov r11, gs:GS_CONTEXT + CONTEXT_R11
    mov r10, gs:GS_CONTEXT + CONTEXT_R10
    mov r9, gs:GS_CONTEXT + CONTEXT_R9
    mov r8, gs:GS_CONTEXT + CONTEXT_R8
    mov rdx, gs:GS_CONTEXT + CONTEXT_RDX
    mov rcx, gs:GS_CONTEXT + CONTEXT_RCX
    mov rax, gs:GS_CONTEXT + CONTEXT_RIP
    mov QWORD PTR [rsp], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RSP
    mov QWORD PTR [rsp+0x18], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RAX

    cmp QWORD PTR [rsp+0x8], 0x8
    je 1f
    swapgs
1:
    iretq
