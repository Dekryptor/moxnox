.intel_syntax noprefix

#include "context.S"

.global page_fault_user_isr
page_fault_user_isr:
    # switch to syscall stack
    mov rax, QWORD PTR gs:GS_CURRENT_THREAD
    mov rax, [rax + THREAD_SYSCALL_STACK]
    sub rax, 0x28
    mov rdx, QWORD PTR [rsp + 0x8] # RIP
    mov QWORD PTR [rax], rdx
    mov rdx, QWORD PTR [rsp + 0x10] # CS
    mov QWORD PTR [rax + 0x8], rdx
    mov rdx, QWORD PTR [rsp + 0x18] # RFLAGS
    mov QWORD PTR [rax + 0x10], rdx
    mov rdx, QWORD PTR [rsp + 0x20] # RSP
    mov QWORD PTR [rax + 0x18], rdx
    mov rdx, QWORD PTR [rsp + 0x28] # SS
    mov QWORD PTR [rax + 0x20], rdx
    pop rdx # error code
    mov rsp, rax

    mov rax, gs:GS_CONTEXT + CONTEXT_R11
    push rax
    mov rax, gs:GS_CONTEXT + CONTEXT_R10
    push rax
    mov rax, gs:GS_CONTEXT + CONTEXT_R9
    push rax
    mov rax, gs:GS_CONTEXT + CONTEXT_R8
    push rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RDX
    push rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RCX
    push rax

    # mark thread as in kernel mode
    mov rax, gs:GS_CURRENT_THREAD
    mov BYTE PTR [rax + THREAD_KERNEL_MODE], 1

    mov rcx, cr2
    sub rsp, 0x20
    sti
    call page_fault_user
    cli
    add rsp, 0x20

    # mark thread as not in kernel mode
    mov rax, gs:GS_CURRENT_THREAD
    mov BYTE PTR [rax + THREAD_KERNEL_MODE], 0

    pop rcx
    pop rdx
    pop r8
    pop r9
    pop r10
    pop r11

    # change to process cr3
    mov rax, gs:GS_CURRENT_THREAD
    mov rax, [rax+THREAD_PROC]
    mov rax, [rax+PROCESS_CR3]
    mov cr3, rax

    mov rax, gs:GS_CONTEXT + CONTEXT_RAX

    swapgs
    iretq

.global page_fault_isr
page_fault_isr:
    cmp QWORD PTR [rsp+0x10], 0x8
    je 0f
    swapgs
    load_kernel_cr3
0:
    save_context_when_error_code

    mov rax, QWORD PTR gs:GS_CURRENT_THREAD
    cmp rax, 0
    je 1f
    cmp QWORD PTR [rax + THREAD_PROC], 0
    je 1f
    cmp BYTE PTR [rax + THREAD_KERNEL_MODE], 0
    jne 1f
    jmp page_fault_user_isr

1:
    mov rcx, cr2
    pop rdx
    sub rsp, 0x20
    call page_fault
2:
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
    je 3f
    load_process_cr3
    swapgs
3:
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
    mov DWORD PTR [APIC_BASE+APIC_EOI], eax
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

.global yield_isr
yield_isr:
    cmp QWORD PTR [rsp+0x8], 0x8
    je 0f
    swapgs
0:
    push rax
    xor rax, rax
    mov DWORD PTR [APIC_BASE+APIC_EOI], eax
    pop rax

    mov rsp, gs:GS_SCHEDULER_STACK
    jmp scheduler
