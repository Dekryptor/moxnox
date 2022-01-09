.intel_syntax noprefix

.text

.global NtDisplayString
NtDisplayString:
    mov r10, rcx
    mov eax, 0x1000
    syscall
    ret

.section .drectve
    .ascii " -export:\"NtDisplayString\""
