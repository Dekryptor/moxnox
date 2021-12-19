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
    mov gs:GS_CONTEXT + CONTEXT_RFLAGS, rax # old RFLAGS
    mov rax, [rsp+0x8]
    mov gs:GS_CONTEXT + CONTEXT_CS, rax # old cs
    mov rax, [rsp+0x20]
    mov gs:GS_CONTEXT + CONTEXT_SS, rax # old ss
    # FIXME - other segments, debug registers, FXSAVE
.endm

.macro save_context_when_error_code # clobbers rax
    mov gs:GS_CONTEXT + CONTEXT_RAX, rax
    mov gs:GS_CONTEXT + CONTEXT_RCX, rcx
    mov gs:GS_CONTEXT + CONTEXT_RDX, rdx
    mov gs:GS_CONTEXT + CONTEXT_RBX, rbx
    mov rax, [rsp+0x20]
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
    mov rax, [rsp+0x8]
    mov gs:GS_CONTEXT + CONTEXT_RIP, rax # old rip
    mov rax, [rsp+0x18]
    mov gs:GS_CONTEXT + CONTEXT_RFLAGS, rax # old RFLAGS
    mov rax, [rsp+0x10]
    mov gs:GS_CONTEXT + CONTEXT_CS, rax # old cs
    mov rax, [rsp+0x28]
    mov gs:GS_CONTEXT + CONTEXT_SS, rax # old ss
    # FIXME - other segments, debug registers, FXSAVE
.endm
