.macro save_context # clobbers rax
    mov gs:0x18, rax
    mov gs:0x20, rcx
    mov gs:0x28, rdx
    mov gs:0x30, rbx
    mov rax, [rsp+0x18]
    mov gs:0x38, rax # old rsp
    mov gs:0x40, rbp
    mov gs:0x48, rsi
    mov gs:0x50, rdi
    mov gs:0x58, r8
    mov gs:0x60, r9
    mov gs:0x68, r10
    mov gs:0x70, r11
    mov gs:0x78, r12
    mov gs:0x80, r13
    mov gs:0x88, r14
    mov gs:0x90, r15
    mov rax, [rsp]
    mov gs:0x98, rax # old rip
    mov rax, [rsp+0x10]
    mov gs:0xa0, rax # old RFLAGS
    mov rax, [rsp+0x8]
    mov gs:0xa8, rax # old cs
    mov rax, [rsp+0x20]
    mov gs:0xb0, rax # old ss
    # FIXME - other segments, debug registers, FXSAVE
.endm

.macro save_context_when_error_code # clobbers rax
    mov gs:0x18, rax
    mov gs:0x20, rcx
    mov gs:0x28, rdx
    mov gs:0x30, rbx
    mov rax, [rsp+0x20]
    mov gs:0x38, rax # old rsp
    mov gs:0x40, rbp
    mov gs:0x48, rsi
    mov gs:0x50, rdi
    mov gs:0x58, r8
    mov gs:0x60, r9
    mov gs:0x68, r10
    mov gs:0x70, r11
    mov gs:0x78, r12
    mov gs:0x80, r13
    mov gs:0x88, r14
    mov gs:0x90, r15
    mov rax, [rsp+0x8]
    mov gs:0x98, rax # old rip
    mov rax, [rsp+0x18]
    mov gs:0xa0, rax # old RFLAGS
    mov rax, [rsp+0x10]
    mov gs:0xa8, rax # old cs
    mov rax, [rsp+0x28]
    mov gs:0xb0, rax # old ss
    # FIXME - other segments, debug registers, FXSAVE
.endm
