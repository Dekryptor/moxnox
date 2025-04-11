#include "context_offsets.h"

.macro save_context # clobbers rax
    mov gs:GS_CONTEXT + CONTEXT_RAX, rax
    mov gs:GS_CONTEXT + CONTEXT_RCX, rcx
    mov gs:GS_CONTEXT + CONTEXT_RDX, rdx
    mov gs:GS_CONTEXT + CONTEXT_RBX, rbx
    mov rax, [rsp+0x18]
    mov gs:GS_CONTEXT + CONTEXT_RSP, rax # old rsp
    mov gs:GS_CONTEXT + CONTEXT_RBP, rbp
    mov gs:GS_CONTEXT + CONTEXT_RSI, rsi
    mov gs:GS_CONTEXT + CONTEXT_RDI, rdi
    mov gs:GS_CONTEXT + CONTEXT_R8, r8
    mov gs:GS_CONTEXT + CONTEXT_R9, r9
    mov gs:GS_CONTEXT + CONTEXT_R10, r10
    mov gs:GS_CONTEXT + CONTEXT_R11, r11
    mov gs:GS_CONTEXT + CONTEXT_R12, r12
    mov gs:GS_CONTEXT + CONTEXT_R13, r13
    mov gs:GS_CONTEXT + CONTEXT_R14, r14
    mov gs:GS_CONTEXT + CONTEXT_R15, r15
    mov rax, [rsp]
    mov gs:GS_CONTEXT + CONTEXT_RIP, rax # old rip
    mov rax, [rsp+0x10]
    mov gs:GS_CONTEXT + CONTEXT_EFLAGS, eax # old RFLAGS
    mov rax, [rsp+0x8]
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_CS, ax # old cs
    mov rax, [rsp+0x20]
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_SS, ax # old ss
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_DS, ds
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_ES, es
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_FS, fs
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_GS, gs
    # FIXME - debug registers
    fxsave gs:GS_CONTEXT + CONTEXT_FXSAVE
.endm

.macro save_context2 # clobbers rax
    mov gs:GS_CONTEXT + CONTEXT_RAX, rax
    mov gs:GS_CONTEXT + CONTEXT_RCX, rcx
    mov gs:GS_CONTEXT + CONTEXT_RDX, rdx
    mov gs:GS_CONTEXT + CONTEXT_RBX, rbx
    mov rax, [rsp+0x38]
    mov gs:GS_CONTEXT + CONTEXT_RSP, rax # old rsp
    mov gs:GS_CONTEXT + CONTEXT_RBP, rbp
    mov gs:GS_CONTEXT + CONTEXT_RSI, rsi
    mov gs:GS_CONTEXT + CONTEXT_RDI, rdi
    mov gs:GS_CONTEXT + CONTEXT_R8, r8
    mov gs:GS_CONTEXT + CONTEXT_R9, r9
    mov gs:GS_CONTEXT + CONTEXT_R10, r10
    mov gs:GS_CONTEXT + CONTEXT_R11, r11
    mov gs:GS_CONTEXT + CONTEXT_R12, r12
    mov gs:GS_CONTEXT + CONTEXT_R13, r13
    mov gs:GS_CONTEXT + CONTEXT_R14, r14
    mov gs:GS_CONTEXT + CONTEXT_R15, r15
    mov rax, [rsp+0x20]
    mov gs:GS_CONTEXT + CONTEXT_RIP, rax # old rip
    mov rax, [rsp+0x30]
    mov gs:GS_CONTEXT + CONTEXT_EFLAGS, eax # old RFLAGS
    mov rax, [rsp+0x28]
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_CS, ax # old cs
    mov rax, [rsp+0x40]
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_SS, ax # old ss
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_DS, ds
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_ES, es
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_FS, fs
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_GS, gs
    # FIXME - debug registers
    fxsave gs:GS_CONTEXT + CONTEXT_FXSAVE
.endm

.macro save_context_when_error_code # clobbers rax
    mov gs:GS_CONTEXT + CONTEXT_RAX, rax
    mov gs:GS_CONTEXT + CONTEXT_RCX, rcx
    mov gs:GS_CONTEXT + CONTEXT_RDX, rdx
    mov gs:GS_CONTEXT + CONTEXT_RBX, rbx
    mov rax, [rsp+0x40]
    mov gs:GS_CONTEXT + CONTEXT_RSP, rax # old rsp
    mov gs:GS_CONTEXT + CONTEXT_RBP, rbp
    mov gs:GS_CONTEXT + CONTEXT_RSI, rsi
    mov gs:GS_CONTEXT + CONTEXT_RDI, rdi
    mov gs:GS_CONTEXT + CONTEXT_R8, r8
    mov gs:GS_CONTEXT + CONTEXT_R9, r9
    mov gs:GS_CONTEXT + CONTEXT_R10, r10
    mov gs:GS_CONTEXT + CONTEXT_R11, r11
    mov gs:GS_CONTEXT + CONTEXT_R12, r12
    mov gs:GS_CONTEXT + CONTEXT_R13, r13
    mov gs:GS_CONTEXT + CONTEXT_R14, r14
    mov gs:GS_CONTEXT + CONTEXT_R15, r15
    mov rax, [rsp+0x28]
    mov gs:GS_CONTEXT + CONTEXT_RIP, rax # old rip
    mov rax, [rsp+0x38]
    mov gs:GS_CONTEXT + CONTEXT_EFLAGS, eax # old RFLAGS
    mov rax, [rsp+0x30]
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_CS, ax # old cs
    mov rax, [rsp+0x48]
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_SS, ax # old ss
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_DS, ds
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_ES, es
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_FS, fs
    mov WORD PTR gs:GS_CONTEXT + CONTEXT_GS, gs
    # FIXME - debug registers
    fxsave gs:GS_CONTEXT + CONTEXT_FXSAVE
.endm

.macro save_syscall_context
    mov gs:GS_SYSCALL_RAX, rax
    mov rax, gs:GS_CURRENT_THREAD
    mov [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_RCX], r10
    mov [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_RDX], rdx

    mov rdx, rax
    mov rax, gs:GS_SYSCALL_RAX
    mov [rdx + THREAD_SYSCALL_CONTEXT + CONTEXT_RAX], rax
    mov rax, gs:GS_CURRENT_THREAD
    mov rdx, [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_RDX]

    mov [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_RBX], rbx
    mov [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_RSP], rsp
    mov [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_RBP], rbp
    mov [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_RSI], rsi
    mov [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_RDI], rdi
    mov [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_R8], r8
    mov [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_R9], r9
    mov QWORD PTR [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_R10], 0 # clobbered by rcx
    mov QWORD PTR [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_R11], 0 # clobbered by rflags
    mov [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_R12], r12
    mov [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_R13], r13
    mov [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_R14], r14
    mov [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_R15], r15
    mov [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_RIP], rcx
    mov [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_EFLAGS], r11
    mov WORD PTR [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_CS], 0x3b
    mov WORD PTR [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_SS], 0x33
    mov WORD PTR [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_DS], ds
    mov WORD PTR [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_ES], es
    mov WORD PTR [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_FS], fs
    mov WORD PTR [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_GS], gs
    # FIXME - debug registers
    fxsave [rax + THREAD_SYSCALL_CONTEXT + CONTEXT_FXSAVE]
.endm

.macro load_kernel_cr3
    push rax
    mov rax, gs:GS_KERNEL_CR3
    mov cr3, rax
    pop rax
.endm

.macro load_process_cr3
    push rax
    mov rax, gs:GS_CURRENT_THREAD
    mov rax, [rax+THREAD_PROC]
    mov rax, [rax+PROCESS_CR3]
    mov cr3, rax
    pop rax
.endm

.macro dynamic_isr num
    .seh_proc "dynamic_isr_\num"
    .global "dynamic_isr_\num"
"dynamic_isr_\num":
    .seh_pushframe
    sub rsp, 0x58
    .seh_stackalloc 0x58
    .seh_endprologue

    cmp QWORD PTR [rsp+0x60], 0x8
    je 0f
    swapgs
    load_kernel_cr3
0:
    mov [rsp+0x50], rax
    mov [rsp+0x48], rcx
    mov [rsp+0x40], rdx
    mov [rsp+0x38], r8
    mov [rsp+0x30], r9
    mov [rsp+0x28], r10
    mov [rsp+0x20], r11

    mov cl, 0x\num
    call handle_dynamic_isr

    xor rax, rax
    mov DWORD PTR [APIC_BASE+APIC_EOI], eax

    mov r11, [rsp+0x20]
    mov r10, [rsp+0x28]
    mov r9, [rsp+0x30]
    mov r8, [rsp+0x38]
    mov rdx, [rsp+0x40]
    mov rcx, [rsp+0x48]
    mov rax, [rsp+0x50]

    cmp QWORD PTR [rsp+0x60], 0x8
    je 1f
    load_process_cr3
    swapgs
1:
    add rsp, 0x58
    iretq
    .seh_endproc
"dynamic_isr_\num" "_end":
.endm

.macro dwarf_func_asm name
    .uleb128 2
    .asciz "\name"
    .quad \name
    .quad \name\()_end - \name
.endm
