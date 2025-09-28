#include "context_offsets.h"

.arch_extension lse

// FIXME - DWARF information

.global scheduler_restore
scheduler_restore:
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
    // x18 done below
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

    ldr x1, [x0, #CONTEXT_PC]
    msr elr_el1, x1

    ldr w1, [x0, #CONTEXT_CPSR] // gets zero-extended
    msr spsr_el1, x1

    // FIXME - debug registers

    ldr x0, [x18, #GS_CURRENT_THREAD]
    add x1, x0, THREAD_USER_STACK
    ldr x1, [x1]
    msr sp_el0, x1

    add x1, x0, THREAD_SYSCALL_STACK
    ldr x1, [x1]
    mov sp, x1

    add x1, x0, THREAD_PROC
    ldr x1, [x1]
    cbz x1, 0f

    // FIXME - should set ttbr0 to 0 for kernel threads if in debug mode? (once free_lower_half_mappings is done)

    // user thread
    add x1, x1, PROCESS_TTBR0
    ldr x1, [x1]
    msr ttbr0_el1, x1

0:
    ldr x0, [x18, #GS_CURRENT_THREAD]
    add x0, x0, THREAD_CONTEXT

    ldr w1, [x0, #CONTEXT_CPSR]
    and x1, x1, 0xf
    cbnz x1, 1f
    // thread is in user mode
    ldr x18, [x0, #CONTEXT_X0 + 0x90]

1:
    ldr x1, [x0, #CONTEXT_X0 + 0x8]
    ldr x0, [x0, #CONTEXT_X0]

    eret

// FIXME - non-LSE version?
.global acquire_sync_lock
acquire_sync_lock:
    adr x0, sync_lock_val
    mov x1, #1
    b 1f
0:
    yield
1:
    swpa x1, x1, [x0]
    cbnz x1, 0b
#ifdef DEBUG
    add x0, x18, GS_CPU_NO
    ldr w1, [x0]
    adr x2, sync_lock_cpu
    str w1, [x2]

    add x0, x18, GS_CURRENT_THREAD
    ldr x1, [x0]
    adr x2, sync_lock_thread
    str x1, [x2]
#endif
    ret

.global release_sync_lock
release_sync_lock:
#ifdef DEBUG
    mov w1, #0xffffffff
    adr x2, sync_lock_cpu
    str w1, [x2]

    adr x2, sync_lock_thread
    str xzr, [x2]
#endif
    adr x0, sync_lock_val
    mov x1, #0
    swpa x1, x1, [x0]
    ret

// x0 has been set to lr before calling this function
.global yield
yield:
sub	sp, sp, #0x10

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

ldr	x1, [sp]
str	x1, [x0, #CONTEXT_X0 + 0xf0] // lr

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

// FIXME - debug registers

mrs	x1, fpcr
str	w1, [x0, #CONTEXT_FPCR]

mrs	x1, fpsr
str	w1, [x0, #CONTEXT_FPSR]

add	x1, sp, #0x10
str	x1, [x0, #CONTEXT_SP]

str	lr, [x0, #CONTEXT_PC]

mrs	x1, spsr_el1
str	w1, [x0, #CONTEXT_CPSR]

add	x0, x18, GS_SCHEDULER_STACK
ldr	x0, [x0]
mov	sp, x0
b	scheduler
