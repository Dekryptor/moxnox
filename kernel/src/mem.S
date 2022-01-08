.intel_syntax noprefix

.equ IA32_PAT_MSR, 0x277

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

# void memcpy_aligned(void* dest, const void* src, size_t n)
# requires src and dest both to be aligned to 16 bytes
.global memcpy_aligned
memcpy_aligned:
    # rcx = dest, rdx = src, r8 = n
    cmp r8, 16
    jl memcpy
    movaps xmm0, [rdx]
    movaps [rcx], xmm0
    add rdx, 16
    add rcx, 16
    sub r8, 16
    jmp memcpy_aligned

.global init_pat
init_pat:
    # FIXME - do we need to check this is available, or is it there for all amd64 CPUs?

    # set PA4 to write-combine
    mov ecx, IA32_PAT_MSR
    rdmsr
    and edx, ~0x7
    or edx, 1
    wrmsr

    # force refresh
    mov rax, cr3
    mov cr3, rax

    ret
