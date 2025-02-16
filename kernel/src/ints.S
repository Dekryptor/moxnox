.intel_syntax noprefix

#include "context.S"
#include "dwarf.S"

.section .debug_abbrev, "dr"

.uleb128 1 # entry num
.uleb128 DW_TAG_compile_unit
.byte 1 # has children
.uleb128 DW_AT_low_pc
.uleb128 DW_FORM_addr
.uleb128 DW_AT_high_pc
.uleb128 DW_FORM_udata
.uleb128 DW_AT_name
.uleb128 DW_FORM_string
.uleb128 DW_AT_language
.uleb128 DW_FORM_data2
.uleb128 0
.uleb128 0

.uleb128 2 # entry num
.uleb128 DW_TAG_subprogram
.byte 0 # has children
.uleb128 DW_AT_name
.uleb128 DW_FORM_string
.uleb128 DW_AT_low_pc
.uleb128 DW_FORM_addr
.uleb128 DW_AT_high_pc
.uleb128 DW_FORM_data8
.uleb128 0
.uleb128 0

.uleb128 0

.section .debug_info, "dr"

.long .debug_info_end - .debug_info_start

.debug_info_start:
.short 5 # version
.byte DW_UT_compile
.byte 8 # address size
.secrel32 .debug_abbrev

.uleb128 1
.quad .text # low_pc
.uleb128 .text_end - .text # high_pc
.asciz __FILE__ # name
.short DW_LANG_Mips_Assembler # language

.uleb128 2
.asciz "page_fault_isr"
.quad page_fault_isr
.quad page_fault_isr_end - page_fault_isr

.uleb128 2
.asciz "double_fault_isr"
.quad double_fault_isr
.quad double_fault_isr_end - double_fault_isr

.uleb128 2
.asciz "general_protection_fault_isr"
.quad general_protection_fault_isr
.quad general_protection_fault_isr_end - general_protection_fault_isr

.uleb128 2
.asciz "invalid_opcode_isr"
.quad invalid_opcode_isr
.quad invalid_opcode_isr_end - invalid_opcode_isr

.uleb128 2
.asciz "spurious_isr"
.quad spurious_isr
.quad spurious_isr_end - spurious_isr

.uleb128 2
.asciz "halt_isr"
.quad halt_isr
.quad halt_isr_end - halt_isr

.uleb128 2
.asciz "divide_by_zero_isr"
.quad divide_by_zero_isr
.quad divide_by_zero_isr_end - divide_by_zero_isr

.uleb128 2
.asciz "yield_isr"
.quad yield_isr
.quad yield_isr_end - yield_isr

.uleb128 2
.asciz "breakpoint_isr"
.quad breakpoint_isr
.quad breakpoint_isr_end - breakpoint_isr

.uleb128 2
.asciz "single_step_isr"
.quad single_step_isr
.quad single_step_isr_end - single_step_isr

.uleb128 0

.debug_info_end:

.text

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
page_fault_isr_end:

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
double_fault_isr_end:

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
general_protection_fault_isr_end:

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
invalid_opcode_isr_end:

.global spurious_isr
spurious_isr:
    push rax
    xor rax, rax
    mov DWORD PTR [APIC_BASE+APIC_EOI], eax
    pop rax
    iretq
spurious_isr_end:

.global halt_isr
halt_isr:
    hlt
    jmp halt_isr
halt_isr_end:

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
divide_by_zero_isr_end:

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
yield_isr_end:

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
breakpoint_isr_end:

.seh_proc single_step_isr
.global single_step_isr
single_step_isr:
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

    # clear TF flag when we return
    mov rax, [rsp+0x30]
    and rax, ~0x100
    mov [rsp+0x30], rax

    call single_step

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
single_step_isr_end:

.text_end:
