.intel_syntax noprefix

# void* memcpy(void* dest, const void* src, size_t n)
.global memcpy
memcpy:
    # rcx = dest, rdx = src, r8 = n
    push rcx

    # FIXME - SSE

0:
    cmp r8, 8
    jl 1f
    mov rax, [rdx]
    mov [rcx], rax
    add rdx, 8
    add rcx, 8
    sub r8, 8
    jmp 0b

1:
    cmp r8, 4
    jl 2f
    mov eax, [rdx]
    mov [rcx], eax
    add rdx, 4
    add rcx, 4
    sub r8, 4
    jmp 1b

2:
    cmp r8, 2
    jl 3f
    mov ax, [rdx]
    mov [rcx], ax
    add rdx, 2
    add rcx, 2
    sub r8, 2
    jmp 2b

3:
    test r8, r8
    je 4f
    mov al, [rdx]
    mov [rcx], al
    inc rdx
    inc rcx
    dec r8
    jmp 3b

4:
    pop rax
    ret
