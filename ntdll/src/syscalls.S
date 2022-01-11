.intel_syntax noprefix

.macro do_syscall name, num
    .text
    .global "\name"
    "\name":
    mov r10, rcx
    mov eax, \num
    syscall
    ret

    .section .drectve
    .ascii " -export:\"\name\""
.endm

do_syscall "NtDisplayString", 0x1000
do_syscall "NtTerminateThread", 0x1001
do_syscall "NtTerminateProcess", 0x1002
do_syscall "NtWriteFile", 0x1003
