// FIXME - DWARF

.text

.macro do_syscall name, num
    .text
    .global "\name"
    "\name":
    svc #\num
    ret
    \name\()_end:

    .section .drectve
    .ascii " -export:\"\name\""
.endm

#include "syscall-list.S"
