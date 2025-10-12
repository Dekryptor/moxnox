.text

// FIXME - DWARF markings

// uint64_t ttbr0_el1_load();
.global ttbr0_el1_load
ttbr0_el1_load:
mrs	x0, ttbr0_el1
ret
