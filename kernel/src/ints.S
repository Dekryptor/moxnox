.intel_syntax noprefix

#include "context.S"

.seh_proc page_fault_isr
.global page_fault_isr
page_fault_isr: # keeps cr3 of process
    .seh_pushframe code
    sub rsp, 0x20
    .seh_stackalloc 32
    .seh_endprologue
    cmp QWORD PTR [rsp+0x30], 0x8
    je 0f
    swapgs
0:
    save_context_when_error_code

    mov rax, QWORD PTR gs:GS_CURRENT_THREAD
    cmp rax, 0
    je 1f
    cmp QWORD PTR [rax + THREAD_PROC], 0
    je 1f
    cmp BYTE PTR [rax + THREAD_KERNEL_MODE], 0
    jne 1f

page_fault_user_isr:
    # switch to syscall stack
    mov rax, QWORD PTR gs:GS_CURRENT_THREAD
    mov rax, [rax + THREAD_SYSCALL_STACK]
    sub rax, 0x50
    mov rdx, QWORD PTR [rsp + 0x28] # RIP
    mov QWORD PTR [rax + 0x28], rdx
    mov rdx, QWORD PTR [rsp + 0x30] # CS
    mov QWORD PTR [rax + 0x30], rdx
    mov rdx, QWORD PTR [rsp + 0x38] # RFLAGS
    mov QWORD PTR [rax + 0x38], rdx
    mov rdx, QWORD PTR [rsp + 0x40] # RSP
    mov QWORD PTR [rax + 0x40], rdx
    mov rdx, QWORD PTR [rsp + 0x48] # SS
    mov QWORD PTR [rax + 0x48], rdx
    mov rdx, QWORD PTR [rsp + 0x20] # error code
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
    call page_fault_user
    add rsp, 0x20

    cmp ax, 0
    jnz page_fault_user_exception

page_fault_user_no_exception:
    # mark thread as not in kernel mode
    mov rax, gs:GS_CURRENT_THREAD
    mov BYTE PTR [rax + THREAD_KERNEL_MODE], 0

    pop rcx
    pop rdx
    pop r8
    pop r9
    pop r10
    pop r11

    mov rax, gs:GS_CONTEXT + CONTEXT_RAX

    swapgs
    jmp page_fault_end

page_fault_user_exception:
    mov rax, gs:GS_CONTEXT + CONTEXT_RCX
    mov [rsp], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RDX
    mov [rsp+0x8], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RIP
    mov [rsp+0x58], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RSP
    mov [rsp+0x70], rax
    jmp page_fault_user_no_exception

1:
    mov rcx, cr2
    mov rdx, [rsp+0x20]
    call page_fault
2:

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
    je page_fault_end
    swapgs

page_fault_end:
    add rsp, 0x28
    iretq
    .seh_endproc

.seh_proc double_fault_isr
.global double_fault_isr
double_fault_isr:
    .seh_pushframe code
    sub rsp, 0x20
    .seh_stackalloc 32
    .seh_endprologue

    cmp QWORD PTR [rsp+0x30], 0x8
    je 0f
    swapgs
    load_kernel_cr3
0:
    save_context_when_error_code
    mov rcx, QWORD PTR [rsp+0x20]
    call double_fault

    cmp QWORD PTR [rsp+0x30], 0x8
    je 1f
    load_process_cr3
    swapgs
1:
    add rsp, 0x28
    iretq
    .seh_endproc

.seh_proc general_protection_fault_isr
.global general_protection_fault_isr
general_protection_fault_isr:
    .seh_pushframe code
    sub rsp, 0x20
    .seh_stackalloc 32
    .seh_endprologue

    cmp QWORD PTR [rsp+0x30], 0x8
    je 0f
    swapgs
    load_kernel_cr3
0:
    save_context_when_error_code
    mov rcx, QWORD PTR [rsp+0x20]
    call general_protection_fault

    mov r11, gs:GS_CONTEXT + CONTEXT_R11
    mov r10, gs:GS_CONTEXT + CONTEXT_R10
    mov r9, gs:GS_CONTEXT + CONTEXT_R9
    mov r8, gs:GS_CONTEXT + CONTEXT_R8
    mov rdx, gs:GS_CONTEXT + CONTEXT_RDX
    mov rcx, gs:GS_CONTEXT + CONTEXT_RCX
    mov rax, gs:GS_CONTEXT + CONTEXT_RIP
    mov QWORD PTR [rsp+0x28], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RSP
    mov QWORD PTR [rsp+0x40], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RAX

    cmp QWORD PTR [rsp+0x30], 0x8
    je 1f
    load_process_cr3
    swapgs
1:
    add rsp, 0x28
    iretq
    .seh_endproc

.seh_proc invalid_opcode_isr
.global invalid_opcode_isr
invalid_opcode_isr:
    .seh_pushframe
    sub rsp, 0x20
    .seh_stackalloc 32
    .seh_endprologue

    cmp QWORD PTR [rsp+0x28], 0x8
    je 0f
    swapgs
    load_kernel_cr3
0:
    save_context2
    call invalid_opcode

    mov r11, gs:GS_CONTEXT + CONTEXT_R11
    mov r10, gs:GS_CONTEXT + CONTEXT_R10
    mov r9, gs:GS_CONTEXT + CONTEXT_R9
    mov r8, gs:GS_CONTEXT + CONTEXT_R8
    mov rdx, gs:GS_CONTEXT + CONTEXT_RDX
    mov rcx, gs:GS_CONTEXT + CONTEXT_RCX
    mov rax, gs:GS_CONTEXT + CONTEXT_RIP
    mov QWORD PTR [rsp+0x20], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RSP
    mov QWORD PTR [rsp+0x38], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RAX

    cmp QWORD PTR [rsp+0x28], 0x8
    je 1f
    load_process_cr3
    swapgs
1:
    add rsp, 0x20
    iretq
    .seh_endproc

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

.seh_proc divide_by_zero_isr
.global divide_by_zero_isr
divide_by_zero_isr:
    .seh_pushframe
    sub rsp, 0x20
    .seh_stackalloc 32
    .seh_endprologue

    cmp QWORD PTR [rsp+0x28], 0x8
    je 0f
    swapgs
    load_kernel_cr3
0:
    save_context2
    call divide_by_zero

    mov r11, gs:GS_CONTEXT + CONTEXT_R11
    mov r10, gs:GS_CONTEXT + CONTEXT_R10
    mov r9, gs:GS_CONTEXT + CONTEXT_R9
    mov r8, gs:GS_CONTEXT + CONTEXT_R8
    mov rdx, gs:GS_CONTEXT + CONTEXT_RDX
    mov rcx, gs:GS_CONTEXT + CONTEXT_RCX
    mov rax, gs:GS_CONTEXT + CONTEXT_RIP
    mov QWORD PTR [rsp+0x20], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RSP
    mov QWORD PTR [rsp+0x38], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RAX

    cmp QWORD PTR [rsp+0x28], 0x8
    je 1f
    load_process_cr3
    swapgs
1:
    add rsp, 0x20
    iretq
    .seh_endproc

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
    call acquire_sync_lock
    jmp scheduler

.seh_proc breakpoint_isr
.global breakpoint_isr
breakpoint_isr:
    .seh_pushframe
    sub rsp, 0x20
    .seh_stackalloc 32
    .seh_endprologue

    cmp QWORD PTR [rsp+0x28], 0x8
    je 0f
    swapgs
    load_kernel_cr3
0:
    save_context2
    call breakpoint

    mov r11, gs:GS_CONTEXT + CONTEXT_R11
    mov r10, gs:GS_CONTEXT + CONTEXT_R10
    mov r9, gs:GS_CONTEXT + CONTEXT_R9
    mov r8, gs:GS_CONTEXT + CONTEXT_R8
    mov rdx, gs:GS_CONTEXT + CONTEXT_RDX
    mov rcx, gs:GS_CONTEXT + CONTEXT_RCX
    mov rax, gs:GS_CONTEXT + CONTEXT_RIP
    mov QWORD PTR [rsp+0x20], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RSP
    mov QWORD PTR [rsp+0x38], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RAX

    cmp QWORD PTR [rsp+0x28], 0x8
    je 1f
    load_process_cr3
    swapgs
1:
    add rsp, 0x20
    iretq
    .seh_endproc
