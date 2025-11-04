.p2align 12 // start at page boundary

.global ap_trampoline
ap_trampoline:
	mov	sp, x0 // load boot stack

	mrs	x0, CurrentEL
	and	x0, x0, 0xc
	sub	x0, x0, 0x8

	// if already in EL1, jump to ap_trampoline_el1
	cbnz	x0, ap_trampoline_el1

	// set HCR_EL2
	mov	x0, #0x80000000 // RW flag (EL1 is 64-bit)
	msr	hcr_el2, x0

	// allow EL0 and EL1 to use timer
	mov	x0, #0x3 // EL1PCTEN, EL1PCEN
	msr	cnthctl_el2, x0

	// drop to EL1
	mov	x0, sp
	msr	sp_el1, x0

	adrp	x0, ap_trampoline_el1
	add	x0, x0, :lo12:ap_trampoline_el1
	msr	elr_el2, x0

	mov	x0, #0x3c5  // EL1 with SP_EL1, interrupts disabled
	msr	spsr_el2, x0
	eret

ap_trampoline_el1:
	tlbi vmalle1
	dsb ish
	isb

	// x1 = ttbr0, x2 = ttbr1, x3 = sctlr, x4 = cpacr, x5 = tcr, x6 = mair
	// x7 = ap_startup

	ldp	x18, x7, [sp]
	ldp	x6, x5, [sp, 0x10]
	ldp	x4, x3, [sp, 0x20]
	ldp	x2, x1, [sp, 0x30]

	msr	ttbr0_el1, x1
	msr	ttbr1_el1, x2

	msr	mair_el1, x6
	msr	cpacr_el1, x4
	msr	tcr_el1, x5
	msr	sctlr_el1, x3
	msr tpidr_el1, x18

	tlbi vmalle1
	dsb ish
	isb

	// jump to ap_startup
	br	x7
