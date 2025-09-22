#include "context_offsets.h"

.equ GICC_EOIR_OFFSET, 0x10

// FIXME - DWARF information

.global scheduler_restore
scheduler_restore:
    // FIXME - userspace (set x18 to TLB, jump to EL0, set ttbr0)

    ldr x0, [x18, #GS_CURRENT_THREAD]
    add x0, x0, THREAD_CONTEXT

    ldr x2, [x0, #CONTEXT_X0 + 0x10]
    ldr x3, [x0, #CONTEXT_X0 + 0x18]
    ldr x4, [x0, #CONTEXT_X0 + 0x20]
    ldr x5, [x0, #CONTEXT_X0 + 0x28]
    ldr x6, [x0, #CONTEXT_X0 + 0x30]
    ldr x7, [x0, #CONTEXT_X0 + 0x38]
    ldr x8, [x0, #CONTEXT_X0 + 0x40]
    ldr x9, [x0, #CONTEXT_X0 + 0x48]
    ldr x10, [x0, #CONTEXT_X0 + 0x50]
    ldr x11, [x0, #CONTEXT_X0 + 0x58]
    ldr x12, [x0, #CONTEXT_X0 + 0x60]
    ldr x13, [x0, #CONTEXT_X0 + 0x68]
    ldr x14, [x0, #CONTEXT_X0 + 0x70]
    ldr x15, [x0, #CONTEXT_X0 + 0x78]
    ldr x16, [x0, #CONTEXT_X0 + 0x80]
    ldr x17, [x0, #CONTEXT_X0 + 0x88]
    // FIXME - x18
    ldr x19, [x0, #CONTEXT_X0 + 0x98]
    ldr x20, [x0, #CONTEXT_X0 + 0xa0]
    ldr x21, [x0, #CONTEXT_X0 + 0xa8]
    ldr x22, [x0, #CONTEXT_X0 + 0xb0]
    ldr x23, [x0, #CONTEXT_X0 + 0xb8]
    ldr x24, [x0, #CONTEXT_X0 + 0xc0]
    ldr x25, [x0, #CONTEXT_X0 + 0xc8]
    ldr x26, [x0, #CONTEXT_X0 + 0xd0]
    ldr x27, [x0, #CONTEXT_X0 + 0xd8]
    ldr x28, [x0, #CONTEXT_X0 + 0xe0]
    ldr x29, [x0, #CONTEXT_X0 + 0xe8]
    ldr x30, [x0, #CONTEXT_X0 + 0xf0]

    ldr q0, [x0, #CONTEXT_V0]
    ldr q1, [x0, #CONTEXT_V0 + 0x10]
    ldr q2, [x0, #CONTEXT_V0 + 0x20]
    ldr q3, [x0, #CONTEXT_V0 + 0x30]
    ldr q4, [x0, #CONTEXT_V0 + 0x40]
    ldr q5, [x0, #CONTEXT_V0 + 0x50]
    ldr q6, [x0, #CONTEXT_V0 + 0x60]
    ldr q7, [x0, #CONTEXT_V0 + 0x70]
    ldr q8, [x0, #CONTEXT_V0 + 0x80]
    ldr q9, [x0, #CONTEXT_V0 + 0x90]
    ldr q10, [x0, #CONTEXT_V0 + 0xa0]
    ldr q11, [x0, #CONTEXT_V0 + 0xb0]
    ldr q12, [x0, #CONTEXT_V0 + 0xc0]
    ldr q13, [x0, #CONTEXT_V0 + 0xd0]
    ldr q14, [x0, #CONTEXT_V0 + 0xe0]
    ldr q15, [x0, #CONTEXT_V0 + 0xf0]
    ldr q16, [x0, #CONTEXT_V0 + 0x100]
    ldr q17, [x0, #CONTEXT_V0 + 0x110]
    ldr q18, [x0, #CONTEXT_V0 + 0x120]
    ldr q19, [x0, #CONTEXT_V0 + 0x130]
    ldr q20, [x0, #CONTEXT_V0 + 0x140]
    ldr q21, [x0, #CONTEXT_V0 + 0x150]
    ldr q22, [x0, #CONTEXT_V0 + 0x160]
    ldr q23, [x0, #CONTEXT_V0 + 0x170]
    ldr q24, [x0, #CONTEXT_V0 + 0x180]
    ldr q25, [x0, #CONTEXT_V0 + 0x190]
    ldr q26, [x0, #CONTEXT_V0 + 0x1a0]
    ldr q27, [x0, #CONTEXT_V0 + 0x1b0]
    ldr q28, [x0, #CONTEXT_V0 + 0x1c0]
    ldr q29, [x0, #CONTEXT_V0 + 0x1d0]
    ldr q30, [x0, #CONTEXT_V0 + 0x1e0]
    ldr q31, [x0, #CONTEXT_V0 + 0x1f0]

    ldr w1, [x0, #CONTEXT_FPCR] // gets zero-extended
    msr fpcr, x1

    ldr w1, [x0, #CONTEXT_FPSR] // gets zero-extended
    msr fpsr, x1

    ldr x1, [x0, #CONTEXT_SP]
    mov sp, x1

    ldr x1, [x0, #CONTEXT_PC]
    msr elr_el1, x1

    ldr w1, [x0, #CONTEXT_CPSR] // gets zero-extended
    msr spsr_el1, x1

    // FIXME - debug registers

    ldr x1, [x0, #CONTEXT_X0 + 0x8]
    ldr x0, [x0, #CONTEXT_X0]

    eret

.global acquire_sync_lock
acquire_sync_lock:
    // FIXME
    ret

.global release_sync_lock
release_sync_lock: # clobbers rax
    // FIXME
    ret

.global get_cpu_block
get_cpu_block:
    mov x0, x18
    ret
get_cpu_block_end:

// branched from kernel_irq_entry
// can only use x0, x1, x2, and x3
.global timer_isr
timer_isr:
// FIXME - timers
// FIXME - wait timeouts
// FIXME - time slices

// start clock again
adr	x1, hardware_tick_rate
ldr	x1, [x1]
msr	cntp_tval_el0, x1
mov	x0, #1
msr	cntp_ctl_el0, x0

// set end-of-interrupt
adr	x0, gicc
ldr	x0, [x0]
add	x0, x0, GICC_EOIR_OFFSET
adr	x1, timer_irq_no
ldr	w1, [x1]
str	w1, [x0]

// finish up and return
ldp	x2, x3, [sp, #0x10]
ldp	x0, x1, [sp]
add	sp, sp, #0x20
eret
