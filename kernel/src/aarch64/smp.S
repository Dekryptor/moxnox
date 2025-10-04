.p2align 12 // start at page boundary

.global ap_trampoline
ap_trampoline:
	mov	sp, x0 // load boot stack

	ldp	x1, x2, [sp]
	msr	ttbr0_el1, x1
	msr	ttbr1_el1, x2

	// FIXME - enable paging
	// FIXME - sctlr_el1
	// FIXME - cpacr_el1
	// FIXME - tcr_el1
	// FIXME - mair
	// FIXME - vbar_el1

	// FIXME - x18

	// FIXME - set scheduler stack
	// FIXME - reduce ap_left
	// FIXME - jump to scheduler

	b	. // FIXME
