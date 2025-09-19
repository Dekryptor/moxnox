#include "context_offsets.h"

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
b	kernel_exception_entry
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

kernel_exception_entry:
sub	sp, sp, #16
stp	x0, x1, [sp]

add	x0, x18, GS_CONTEXT
stp	x2, x3, [x0, #CONTEXT_X0 + 0x10]
stp	x4, x5, [x0, #CONTEXT_X0 + 0x20]
stp	x6, x7, [x0, #CONTEXT_X0 + 0x30]
stp	x8, x9, [x0, #CONTEXT_X0 + 0x40]
stp	x10, x11, [x0, #CONTEXT_X0 + 0x50]
stp	x12, x13, [x0, #CONTEXT_X0 + 0x60]
stp	x14, x15, [x0, #CONTEXT_X0 + 0x70]
stp	x16, x17, [x0, #CONTEXT_X0 + 0x80]
stp	x18, x19, [x0, #CONTEXT_X0 + 0x90]
stp	x20, x21, [x0, #CONTEXT_X0 + 0xa0]
stp	x22, x23, [x0, #CONTEXT_X0 + 0xb0]
stp	x24, x25, [x0, #CONTEXT_X0 + 0xc0]
stp	x26, x27, [x0, #CONTEXT_X0 + 0xd0]
stp	x28, fp, [x0, #CONTEXT_X0 + 0xe0]
str	lr, [x0, #CONTEXT_X0 + 0xf0]

str	q0, [x0, #CONTEXT_V0]
str	q1, [x0, #CONTEXT_V0 + 0x10]
str	q2, [x0, #CONTEXT_V0 + 0x20]
str	q3, [x0, #CONTEXT_V0 + 0x30]
str	q4, [x0, #CONTEXT_V0 + 0x40]
str	q5, [x0, #CONTEXT_V0 + 0x50]
str	q6, [x0, #CONTEXT_V0 + 0x60]
str	q7, [x0, #CONTEXT_V0 + 0x70]
str	q8, [x0, #CONTEXT_V0 + 0x80]
str	q9, [x0, #CONTEXT_V0 + 0x90]
str	q10, [x0, #CONTEXT_V0 + 0xa0]
str	q11, [x0, #CONTEXT_V0 + 0xb0]
str	q12, [x0, #CONTEXT_V0 + 0xc0]
str	q13, [x0, #CONTEXT_V0 + 0xd0]
str	q14, [x0, #CONTEXT_V0 + 0xe0]
str	q15, [x0, #CONTEXT_V0 + 0xf0]
str	q16, [x0, #CONTEXT_V0 + 0x100]
str	q17, [x0, #CONTEXT_V0 + 0x110]
str	q18, [x0, #CONTEXT_V0 + 0x120]
str	q19, [x0, #CONTEXT_V0 + 0x130]
str	q20, [x0, #CONTEXT_V0 + 0x140]
str	q21, [x0, #CONTEXT_V0 + 0x150]
str	q22, [x0, #CONTEXT_V0 + 0x160]
str	q23, [x0, #CONTEXT_V0 + 0x170]
str	q24, [x0, #CONTEXT_V0 + 0x180]
str	q25, [x0, #CONTEXT_V0 + 0x190]
str	q26, [x0, #CONTEXT_V0 + 0x1a0]
str	q27, [x0, #CONTEXT_V0 + 0x1b0]
str	q28, [x0, #CONTEXT_V0 + 0x1c0]
str	q29, [x0, #CONTEXT_V0 + 0x1d0]
str	q30, [x0, #CONTEXT_V0 + 0x1e0]
str	q31, [x0, #CONTEXT_V0 + 0x1f0]

mrs	x1, fpcr
str	w1, [x0, #CONTEXT_FPCR]

mrs	x1, fpsr
str	w1, [x0, #CONTEXT_FPSR]

add	x1, sp, #16
str	x1, [x0, #CONTEXT_SP]

mrs	x1, elr_el1
str	x1, [x0, #CONTEXT_PC]

mrs	x1, spsr_el1
str	w1, [x0, #CONTEXT_CPSR]

// FIXME - debug registers

ldr	x1, [sp, #8]
str	x1, [x0, #CONTEXT_X0 + 0x8]
ldr	x1, [sp]
str	x1, [x0, #CONTEXT_X0]

mrs	x0, esr_el1
b	handle_kernel_exception
