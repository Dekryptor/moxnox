.equ SPSR_EL1_WITH_SP_EL1, 0x5

.equ HCR_RW, 1 << 31

.text

// uint8_t get_current_el();
.global get_current_el
get_current_el:
mrs	x0, CurrentEL
asr	x0, x0, #2
and	x0, x0, #3
ret

// void el2_to_el1();
.global el2_to_el1
el2_to_el1:
// set EL1 stack
mov	x0, sp
msr	sp_el1, x0

// set destination address
msr	elr_el2, lr

// set status register
mov	x0, SPSR_EL1_WITH_SP_EL1
msr	spsr_el2, x0

// set EL1 to 64-bit mode
mrs	x0, hcr_el2
orr	x0, x0, HCR_RW
msr	hcr_el2, x0

eret

// void el3_to_el1();
.global el3_to_el1
el3_to_el1:
// set EL1 stack
mov	x0, sp
msr	sp_el1, x0

// set destination address
msr	elr_el3, lr

// set status register
mov	x0, SPSR_EL1_WITH_SP_EL1
msr	spsr_el3, x0

// set EL1 to 64-bit mode
mrs	x0, hcr_el2
orr	x0, x0, HCR_RW
msr	hcr_el2, x0

eret
