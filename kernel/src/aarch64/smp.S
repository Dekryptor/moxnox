.p2align 12 // start at page boundary

.global ap_trampoline
ap_trampoline:
	mov	sp, x0 // load boot stack

	// x1 = ttbr0, x2 = ttbr1, x3 = sctlr, x4 = cpacr, x5 = tcr, x6 = mair
	// x7 = ap_startup

	ldp xzr, x7, [sp]
	ldp	x18, xzr, [sp, 0x10]
	ldp	x6, x5, [sp, 0x20]
	ldp	x4, x3, [sp, 0x30]
	ldp	x2, x1, [sp, 0x40]

	msr	ttbr0_el1, x1
	msr	ttbr1_el1, x2

	msr	mair_el1, x6
	msr	cpacr_el1, x4
	msr	tcr_el1, x5
	msr	sctlr_el1, x3
	msr tpidr_el1, x18

	// jump to ap_startup
	br	x7
