// FIXME - DWARF
// FIXME - SEH(?)

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
b	exception_entry
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

exception_entry:
sub	sp, sp, #192
stp	x0, x1, [sp, #0]
stp	x2, x3, [sp, #16]
stp	x4, x5, [sp, #32]
stp	x6, x7, [sp, #48]
stp	x8, x9, [sp, #64]
stp	x10, x11, [sp, #80]
stp	x12, x13, [sp, #96]
stp	x14, x15, [sp, #112]
stp	x16, x17, [sp, #128]
stp	x18, x29, [sp, #144]
stp	x30, xzr, [sp, #160]

// FIXME - vector registers
bl	handle_exception

ldp	x30, xzr, [sp, #160]
ldp	x18, x29, [sp, #144]
ldp	x16, x17, [sp, #128]
ldp	x14, x15, [sp, #112]
ldp	x12, x13, [sp, #96]
ldp	x10, x11, [sp, #80]
ldp	x8, x9, [sp, #64]
ldp	x6, x7, [sp, #48]
ldp	x4, x5, [sp, #32]
ldp	x2, x3, [sp, #16]
ldp	x0, x1, [sp, #0]
add	sp, sp, #192
eret
