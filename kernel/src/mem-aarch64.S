.text

// FIXME - DWARF markings

// uint64_t ttbr1_el1_load();
.global ttbr1_el1_load
ttbr1_el1_load:
mrs	x0, ttbr1_el1
ret

// uint64_t ttbr0_el1_load();
.global ttbr0_el1_load
ttbr0_el1_load:
mrs	x0, ttbr0_el1
ret
