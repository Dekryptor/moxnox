.section .text.interrupts

.balign 2048
.global vector_table
vector_table:
// Synchronous Exception, EL1 using SP_EL0 stack
b	.
.balign 128
// IRQ, EL1 using SP_EL0 stack
b	.
.balign 128
// FIQ, EL1 using SP_EL0 stack
b	.
.balign 128
// SError, EL1 using SP_EL0 stack
b	.
.balign 128
// Synchronous Exception, EL1 using SP_EL1 stack
b	.
.balign 128
// IRQ, EL1 using SP_EL1 stack
b	.
.balign 128
// FIQ, EL1 using SP_EL1 stack
b	.
.balign 128
// SError, EL1 using SP_EL1 stack
b	.
.balign 128
// Synchronous Exception, 64-bit EL0
b	.
.balign 128
// IRQ, 64-bit EL0
b	.
.balign 128
// FIQ, 64-bit EL0
b	.
.balign 128
// SError, 64-bit EL0
b	.
.balign 128
// Synchronous Exception, 32-bit EL0
b	.
.balign 128
// IRQ, 32-bit EL0
b	.
.balign 128
// FIQ, 32-bit EL0
b	.
.balign 128
// SError, 32-bit EL0
b	.
.balign 128

.section .text

// void load_vector_table();
.global load_vector_table
load_vector_table:
adr	x0, vector_table
msr	vbar_el1, x0
ret
