.intel_syntax noprefix

.global NtDisplayString
NtDisplayString:
    mov r10, rcx
    mov eax, 0x1000
    syscall
    ret
