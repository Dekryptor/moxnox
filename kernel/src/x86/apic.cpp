module;

#include "../context_offsets.h"
#include <stdint.h>

export module apic;

export volatile uint8_t* const local_apic = (uint8_t*)APIC_BASE;

export const uint8_t yield_interrupt = 0xfc;
