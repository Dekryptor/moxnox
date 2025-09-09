.text

// FIXME - DWARF markings

// uint64_t ttbr1_el1_load();
.global ttbr1_el1_load
ttbr1_el1_load:
mrs	x0, ttbr1_el1
ret
