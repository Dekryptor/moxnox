#include "../context_offsets.h"
#include "../dwarf.S"

.arch_extension lse

.section .debug_abbrev, "dr"

.uleb128 1 // entry num
.uleb128 DW_TAG_compile_unit
.byte 1 // has children
.uleb128 DW_AT_low_pc
.uleb128 DW_FORM_addr
.uleb128 DW_AT_high_pc
.uleb128 DW_FORM_udata
.uleb128 DW_AT_name
.uleb128 DW_FORM_string
.uleb128 DW_AT_language
.uleb128 DW_FORM_data2
.uleb128 0
.uleb128 0

.uleb128 2 // entry num
.uleb128 DW_TAG_subprogram
.byte 0 // has children
.uleb128 DW_AT_name
.uleb128 DW_FORM_string
.uleb128 DW_AT_low_pc
.uleb128 DW_FORM_addr
.uleb128 DW_AT_high_pc
.uleb128 DW_FORM_data8
.uleb128 0
.uleb128 0

.uleb128 0

.section .debug_info, "dr"

.long .debug_info_end - .debug_info_start

.debug_info_start:
.short 5 // version
.byte DW_UT_compile
.byte 8 // address size
.secrel32 .debug_abbrev

.uleb128 1
.quad .text // low_pc
.uleb128 .text_end - .text // high_pc
.asciz __FILE__ // name
.short DW_LANG_Mips_Assembler // language

dwarf_func_asm scheduler_restore
dwarf_func_asm acquire_sync_lock
dwarf_func_asm release_sync_lock
dwarf_func_asm yield

.uleb128 0

.debug_info_end:

.text

.global scheduler_restore
scheduler_restore:
    ldr x0, [x18, #GS_CURRENT_THREAD]
    add x0, x0, THREAD_CONTEXT

    ldp x2, x3, [x0, #CONTEXT_X0 + 0x10]
    ldp x4, x5, [x0, #CONTEXT_X0 + 0x20]
    ldp x6, x7, [x0, #CONTEXT_X0 + 0x30]
    ldp x8, x9, [x0, #CONTEXT_X0 + 0x40]
    ldp x10, x11, [x0, #CONTEXT_X0 + 0x50]
    ldp x12, x13, [x0, #CONTEXT_X0 + 0x60]
    ldp x14, x15, [x0, #CONTEXT_X0 + 0x70]
    ldp x16, x17, [x0, #CONTEXT_X0 + 0x80]
    // x18 done below
    ldr x19, [x0, #CONTEXT_X0 + 0x98]
    ldp x20, x21, [x0, #CONTEXT_X0 + 0xa0]
    ldp x22, x23, [x0, #CONTEXT_X0 + 0xb0]
    ldp x24, x25, [x0, #CONTEXT_X0 + 0xc0]
    ldp x26, x27, [x0, #CONTEXT_X0 + 0xd0]
    ldp x28, x29, [x0, #CONTEXT_X0 + 0xe0]
    ldr x30, [x0, #CONTEXT_X0 + 0xf0]

    ldp q0, q1, [x0, #CONTEXT_V0]
    ldp q2, q3, [x0, #CONTEXT_V0 + 0x20]
    ldp q4, q5, [x0, #CONTEXT_V0 + 0x40]
    ldp q6, q7, [x0, #CONTEXT_V0 + 0x60]
    ldp q8, q9, [x0, #CONTEXT_V0 + 0x80]
    ldp q10, q11, [x0, #CONTEXT_V0 + 0xa0]
    ldp q12, q13, [x0, #CONTEXT_V0 + 0xc0]
    ldp q14, q15, [x0, #CONTEXT_V0 + 0xe0]
    ldp q16, q17, [x0, #CONTEXT_V0 + 0x100]
    ldp q18, q19, [x0, #CONTEXT_V0 + 0x120]
    ldp q20, q21, [x0, #CONTEXT_V0 + 0x140]
    ldp q22, q23, [x0, #CONTEXT_V0 + 0x160]
    ldp q24, q25, [x0, #CONTEXT_V0 + 0x180]
    ldp q26, q27, [x0, #CONTEXT_V0 + 0x1a0]
    ldp q28, q29, [x0, #CONTEXT_V0 + 0x1c0]
    ldp q30, q31, [x0, #CONTEXT_V0 + 0x1e0]

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
    tlbi vmalle1
    dsb ish
    isb

0:
    ldr x0, [x18, #GS_CURRENT_THREAD]
    add x0, x0, THREAD_CONTEXT

    ldr w1, [x0, #CONTEXT_CPSR]
    and x1, x1, 0xf
    cbnz x1, 1f
    // thread is in user mode
    ldr x18, [x0, #CONTEXT_X0 + 0x90]

1:
    ldp x0, x1, [x0, #CONTEXT_X0]

    eret
scheduler_restore_end:

// FIXME - non-LSE version?
.global acquire_sync_lock
acquire_sync_lock:
    adrp x0, sync_lock_val
    add x0, x0, :lo12:sync_lock_val
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
    adrp x2, sync_lock_cpu
    add x2, x2, :lo12:sync_lock_cpu
    str w1, [x2]

    add x0, x18, GS_CURRENT_THREAD
    ldr x1, [x0]
    adrp x2, sync_lock_thread
    add x2, x2, :lo12:sync_lock_thread
    str x1, [x2]
#endif
    ret
acquire_sync_lock_end:

.global release_sync_lock
release_sync_lock:
#ifdef DEBUG
    mov w1, #0xffffffff
    adrp x2, sync_lock_cpu
    add x2, x2, :lo12:sync_lock_cpu
    str w1, [x2]

    adrp x2, sync_lock_thread
    add x2, x2, :lo12:sync_lock_thread
    str xzr, [x2]
#endif
    adrp x0, sync_lock_val
    add x0, x0, :lo12:sync_lock_val
    mov x1, #0
    swpa x1, x1, [x0]
    ret
release_sync_lock_end:

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

stp	q0, q1, [x0, #CONTEXT_V0]
stp	q2, q3, [x0, #CONTEXT_V0 + 0x20]
stp	q4, q5, [x0, #CONTEXT_V0 + 0x40]
stp	q6, q7, [x0, #CONTEXT_V0 + 0x60]
stp	q8, q9, [x0, #CONTEXT_V0 + 0x80]
stp	q10, q11, [x0, #CONTEXT_V0 + 0xa0]
stp	q12, q13, [x0, #CONTEXT_V0 + 0xc0]
stp	q14, q15, [x0, #CONTEXT_V0 + 0xe0]
stp	q16, q17, [x0, #CONTEXT_V0 + 0x100]
stp	q18, q19, [x0, #CONTEXT_V0 + 0x120]
stp	q20, q21, [x0, #CONTEXT_V0 + 0x140]
stp	q22, q23, [x0, #CONTEXT_V0 + 0x160]
stp	q24, q25, [x0, #CONTEXT_V0 + 0x180]
stp	q26, q27, [x0, #CONTEXT_V0 + 0x1a0]
stp	q28, q29, [x0, #CONTEXT_V0 + 0x1c0]
stp	q30, q31, [x0, #CONTEXT_V0 + 0x1e0]

// FIXME - debug registers

mrs	x1, fpcr
str	w1, [x0, #CONTEXT_FPCR]

mrs	x1, fpsr
str	w1, [x0, #CONTEXT_FPSR]

str	lr, [x0, #CONTEXT_PC]

// FIXME - is there an easy way to get the actual CPSR?
mrs	x1, nzcv
add	x1, x1, #0x5 // EL1 with SP_EL1
str	w1, [x0, #CONTEXT_CPSR]

add	x0, x18, GS_CURRENT_THREAD
ldr	x0, [x0]
add	x2, x0, THREAD_SYSCALL_STACK
add	x1, sp, #0x10
str	x1, [x2]
add	x2, x0, THREAD_USER_STACK
mrs	x1, sp_el0
str	x1, [x2]

add	x0, x18, GS_SCHEDULER_STACK
ldr	x0, [x0]
mov	sp, x0
b	scheduler
yield_end:

.text_end:
