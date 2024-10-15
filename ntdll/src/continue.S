.intel_syntax noprefix

#include "../../kernel/src/context_offsets.h"

# NTSTATUS NtContinue(const CONTEXT* thread_context, bool raise_alert);
.global NtContinue
NtContinue:
    # FIXME - raise_alert
    # FIXME - debug registers

    mov ds, WORD PTR [rcx + CONTEXT_DS]
    mov es, WORD PTR [rcx + CONTEXT_DS]
    mov fs, WORD PTR [rcx + CONTEXT_DS]

    fxrstor [rcx + CONTEXT_FXSAVE]

    mov rdx, QWORD PTR [rcx + CONTEXT_RDX]
    mov rbx, QWORD PTR [rcx + CONTEXT_RBX]
    mov rbp, QWORD PTR [rcx + CONTEXT_RBP]
    mov rsi, QWORD PTR [rcx + CONTEXT_RSI]
    mov rdi, QWORD PTR [rcx + CONTEXT_RDI]
    mov r8, QWORD PTR [rcx + CONTEXT_R8]
    mov r9, QWORD PTR [rcx + CONTEXT_R9]
    mov r10, QWORD PTR [rcx + CONTEXT_R10]
    mov r11, QWORD PTR [rcx + CONTEXT_R11]
    mov r12, QWORD PTR [rcx + CONTEXT_R12]
    mov r13, QWORD PTR [rcx + CONTEXT_R13]
    mov r14, QWORD PTR [rcx + CONTEXT_R14]
    mov r15, QWORD PTR [rcx + CONTEXT_R15]

    xor eax, eax
    mov ax, WORD PTR [rcx + CONTEXT_SS]
    push rax
    push QWORD PTR [rcx + CONTEXT_RSP]
    mov eax, DWORD PTR [rcx + CONTEXT_EFLAGS]
    push rax
    xor eax, eax
    mov ax, WORD PTR [rcx + CONTEXT_CS]
    push rax
    push QWORD PTR [rcx + CONTEXT_RIP]

    mov rax, QWORD PTR [rcx + CONTEXT_RAX]
    mov rcx, QWORD PTR [rcx + CONTEXT_RCX]

    iretq
