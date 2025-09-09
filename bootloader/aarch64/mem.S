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

// uint64_t tcr_el1_load();
.global tcr_el1_load
tcr_el1_load:
mrs	x0, tcr_el1
ret

// void tcr_el1_store(uint64_t tcr_el1);
.global tcr_el1_store
tcr_el1_store:
msr	tcr_el1, x0
ret

// void enable_paging(uint64_t ttbr0, uint64_t ttbr1);
.global enable_paging
enable_paging:
msr	ttbr0_el1, x0
msr	ttbr1_el1, x1
isb

ret

// uint64_t cpacr_el1_load();
.global cpacr_el1_load
cpacr_el1_load:
mrs	x0, cpacr_el1
ret

// void cpacr_el1_store(uint64_t cpacr_el1);
.global cpacr_el1_store
cpacr_el1_store:
msr	cpacr_el1, x0
ret

// uint64_t sctlr_el1_load();
.global sctlr_el1_load
sctlr_el1_load:
mrs	x0, sctlr_el1
ret

// void sctlr_el1_store(uint64_t sctlr_el1);
.global sctlr_el1_store
sctlr_el1_store:
msr	sctlr_el1, x0
ret

// void mair_el1_store(uint64_t mair_el1);
.global mair_el1_store
mair_el1_store:
msr	mair_el1, x0
ret
