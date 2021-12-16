.macro save_context # clobbers rax
    mov [rip+thread_context], rax
    mov [rip+thread_context+0x08], rcx
    mov [rip+thread_context+0x10], rdx
    mov [rip+thread_context+0x18], rbx
    mov rax, [rsp+0x18]
    mov [rip+thread_context+0x20], rax # old rsp
    mov [rip+thread_context+0x28], rbp
    mov [rip+thread_context+0x30], rsi
    mov [rip+thread_context+0x38], rdi
    mov [rip+thread_context+0x40], r8
    mov [rip+thread_context+0x48], r9
    mov [rip+thread_context+0x50], r10
    mov [rip+thread_context+0x58], r11
    mov [rip+thread_context+0x60], r12
    mov [rip+thread_context+0x68], r13
    mov [rip+thread_context+0x70], r14
    mov [rip+thread_context+0x78], r15
    mov rax, [rsp]
    mov [rip+thread_context+0x80], rax # old rip
    mov rax, [rsp+0x10]
    mov [rip+thread_context+0x88], rax # old RFLAGS
    mov rax, [rsp+0x8]
    mov [rip+thread_context+0x90], rax # old cs
    mov rax, [rsp+0x20]
    mov [rip+thread_context+0x98], rax # old ss
    # FIXME - other segments, debug registers, FXSAVE
.endm

.macro save_context_when_error_code # clobbers rax
    mov [rip+thread_context], rax
    mov [rip+thread_context+0x08], rcx
    mov [rip+thread_context+0x10], rdx
    mov [rip+thread_context+0x18], rbx
    mov rax, [rsp+0x20]
    mov [rip+thread_context+0x20], rax # old rsp
    mov [rip+thread_context+0x28], rbp
    mov [rip+thread_context+0x30], rsi
    mov [rip+thread_context+0x38], rdi
    mov [rip+thread_context+0x40], r8
    mov [rip+thread_context+0x48], r9
    mov [rip+thread_context+0x50], r10
    mov [rip+thread_context+0x58], r11
    mov [rip+thread_context+0x60], r12
    mov [rip+thread_context+0x68], r13
    mov [rip+thread_context+0x70], r14
    mov [rip+thread_context+0x78], r15
    mov rax, [rsp+0x8]
    mov [rip+thread_context+0x80], rax # old rip
    mov rax, [rsp+0x18]
    mov [rip+thread_context+0x88], rax # old RFLAGS
    mov rax, [rsp+0x10]
    mov [rip+thread_context+0x90], rax # old cs
    mov rax, [rsp+0x28]
    mov [rip+thread_context+0x98], rax # old ss
    # FIXME - other segments, debug registers, FXSAVE
.endm
