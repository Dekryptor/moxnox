#include "../../kernel/src/context_offsets.h"
#include "exception_record_offsets.h"
#include "dwarf.S"

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

dwarf_func_asm RtlCaptureContext
dwarf_func_asm RtlRaiseException

.uleb128 0

.debug_info_end:

.text

# NTSTATUS RtlCaptureContext(CONTEXT* context);
.global RtlCaptureContext
RtlCaptureContext:
	stp	xzr, x1, [x0, #CONTEXT_X0] // x0 has been clobbered
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
	str	xzr, [x0, #CONTEXT_X0 + 0xf0] // lr has been clobbered

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

	mrs	x1, fpcr
	str	w1, [x0, #CONTEXT_FPCR]

	mrs	x1, fpsr
	str	w1, [x0, #CONTEXT_FPSR]

	mrs	x1, nzcv
	str	w8, [x0, #CONTEXT_CPSR]

	mov	x1, sp
	str	x1, [x0, #CONTEXT_SP]

	str	lr, [x0, #CONTEXT_PC]

	// FIXME - debug registers?
	// FIXME - set ContextFlags

	// return STATUS_SUCCESS
	mov x0, xzr
	ret
RtlCaptureContext_end:

# void RtlRaiseException(EXCEPTION_RECORD* exception_record);
.seh_proc RtlRaiseException
.global RtlRaiseException
RtlRaiseException:
	sub	sp, sp, #0x3a0
	.seh_stackalloc 0x3a0

	// stack:
	// exception_record (sp)
	// lr (sp + 0x8)
	// context (sp + 0x10)

	stp	x0, lr, [sp]
	.seh_save_reg lr, 8
	.seh_endprologue

	add	x0, sp, #0x10
	bl	RtlCaptureContext

	// put lr as exception_record->ExceptionAddress
	ldr	x1, [sp, #0x8]
	ldr	x0, [sp]
	str	x1, [x0, #EXCEPTION_RECORD_EXCEPTION_ADDRESS]

	// x0 is already address of exception_record
	add	x1, sp, #0x10
	b	mn_signal
.seh_endproc
RtlRaiseException_end:

.text_end:
