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

dwarf_func_asm page_fault_isr
dwarf_func_asm double_fault_isr
dwarf_func_asm general_protection_fault_isr
dwarf_func_asm invalid_opcode_isr
dwarf_func_asm spurious_isr
dwarf_func_asm halt_isr
dwarf_func_asm divide_by_zero_isr
dwarf_func_asm yield_isr
dwarf_func_asm breakpoint_isr
dwarf_func_asm single_step_isr

dwarf_func_asm dynamic_isr_30
dwarf_func_asm dynamic_isr_31
dwarf_func_asm dynamic_isr_32
dwarf_func_asm dynamic_isr_33
dwarf_func_asm dynamic_isr_34
dwarf_func_asm dynamic_isr_35
dwarf_func_asm dynamic_isr_36
dwarf_func_asm dynamic_isr_37
dwarf_func_asm dynamic_isr_38
dwarf_func_asm dynamic_isr_39
dwarf_func_asm dynamic_isr_3a
dwarf_func_asm dynamic_isr_3b
dwarf_func_asm dynamic_isr_3c
dwarf_func_asm dynamic_isr_3d
dwarf_func_asm dynamic_isr_3e
dwarf_func_asm dynamic_isr_3f
dwarf_func_asm dynamic_isr_40
dwarf_func_asm dynamic_isr_41
dwarf_func_asm dynamic_isr_42
dwarf_func_asm dynamic_isr_43
dwarf_func_asm dynamic_isr_44
dwarf_func_asm dynamic_isr_45
dwarf_func_asm dynamic_isr_46
dwarf_func_asm dynamic_isr_47
dwarf_func_asm dynamic_isr_48
dwarf_func_asm dynamic_isr_49
dwarf_func_asm dynamic_isr_4a
dwarf_func_asm dynamic_isr_4b
dwarf_func_asm dynamic_isr_4c
dwarf_func_asm dynamic_isr_4d
dwarf_func_asm dynamic_isr_4e
dwarf_func_asm dynamic_isr_4f

.uleb128 0

.debug_info_end:

.bss

.global processors_left_to_halt
processors_left_to_halt:
.space 4

.text

.seh_proc page_fault_isr
.global page_fault_isr
page_fault_isr: # keeps cr3 of process
#ifdef __llvm__
    .seh_pushframe @code
#else
    .seh_pushframe code
#endif
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

.macro exception_isr name
    .seh_proc \name\()_isr
    .global \name\()_isr
\name\()_isr:
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

    mov rax, QWORD PTR gs:GS_CURRENT_THREAD
    cmp rax, 0
    je 1f
    cmp QWORD PTR [rax + THREAD_PROC], 0
    je 1f
    cmp BYTE PTR [rax + THREAD_KERNEL_MODE], 0
    jne 1f

    # userspace version
    # switch to syscall stack
    mov rax, [rax + THREAD_SYSCALL_STACK]
    sub rax, 0x50 # FIXME
    mov rdx, QWORD PTR [rsp + 0x20] # RIP
    mov QWORD PTR [rax + 0x20], rdx
    mov rdx, QWORD PTR [rsp + 0x28] # CS
    mov QWORD PTR [rax + 0x28], rdx
    mov rdx, QWORD PTR [rsp + 0x30] # RFLAGS
    mov QWORD PTR [rax + 0x30], rdx
    mov rdx, QWORD PTR [rsp + 0x38] # RSP
    mov QWORD PTR [rax + 0x38], rdx
    mov rdx, QWORD PTR [rsp + 0x40] # SS
    mov QWORD PTR [rax + 0x40], rdx
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

    sub rsp, 0x20
    call \name\()_user
    add rsp, 0x20

    mov rax, gs:GS_CONTEXT + CONTEXT_RCX
    mov [rsp], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RDX
    mov [rsp+0x8], rax
    mov rax, gs:GS_CONTEXT + CONTEXT_RIP
    mov [rsp+0x50], rax
    mov eax, gs:GS_CONTEXT + CONTEXT_EFLAGS
    mov [rsp+0x60], eax
    mov rax, gs:GS_CONTEXT + CONTEXT_RSP
    mov [rsp+0x68], rax

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

    load_process_cr3
    swapgs
    jmp 2f

1:
    # kernelspace version
    call \name\()_kernel

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

2:
    add rsp, 0x20
    iretq
    .seh_endproc
\name\()_isr_end:
.endm

exception_isr "invalid_opcode"
exception_isr "breakpoint"
exception_isr "single_step"

.seh_proc double_fault_isr
.global double_fault_isr
double_fault_isr:
#ifdef __llvm__
    .seh_pushframe @code
#else
    .seh_pushframe code
#endif
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
#ifdef __llvm__
    .seh_pushframe @code
#else
    .seh_pushframe code
#endif
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
    lock dec DWORD PTR [rip+processors_left_to_halt]
0:
    hlt
    jmp 0b
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

dynamic_isr 30
dynamic_isr 31
dynamic_isr 32
dynamic_isr 33
dynamic_isr 34
dynamic_isr 35
dynamic_isr 36
dynamic_isr 37
dynamic_isr 38
dynamic_isr 39
dynamic_isr 3a
dynamic_isr 3b
dynamic_isr 3c
dynamic_isr 3d
dynamic_isr 3e
dynamic_isr 3f
dynamic_isr 40
dynamic_isr 41
dynamic_isr 42
dynamic_isr 43
dynamic_isr 44
dynamic_isr 45
dynamic_isr 46
dynamic_isr 47
dynamic_isr 48
dynamic_isr 49
dynamic_isr 4a
dynamic_isr 4b
dynamic_isr 4c
dynamic_isr 4d
dynamic_isr 4e
dynamic_isr 4f

.text_end:
