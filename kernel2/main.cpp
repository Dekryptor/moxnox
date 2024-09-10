#include <stdbool.h>
#include <stdint.h>

struct gdt_entry {
	uint16_t LimitLow;
	uint16_t BaseLow;
	uint32_t BaseMid:8;
	uint32_t Type:5;
	uint32_t Dpl:2;
	uint32_t Pres:1;
	uint32_t LimitHi:4;
	uint32_t Sys:1;
	uint32_t Long:1;
	uint32_t Default_Big:1;
	uint32_t Granularity:1;
	uint32_t BaseHi:8;
};

static_assert(sizeof(gdt_entry) == 8);

static consteval gdt_entry make_gdt_entry(uint32_t base, uint32_t limit, uint8_t type,
										  uint8_t dpl, bool pres, bool sys, bool long_mode,
										  bool default_big, bool granularity) {
	gdt_entry g;

	g.LimitLow = limit & 0xffff;
	g.LimitHi = (limit >> 16) & 0xf;

	g.BaseLow = base & 0xffff;
	g.BaseMid = (base >> 16) & 0xff;
	g.BaseHi = (base >> 24) & 0xff;

	g.Type = type;
	g.Dpl = dpl;
	g.Pres = pres ? 1 : 0;

	g.Sys = sys;
	g.Long = long_mode;
	g.Default_Big = default_big;
	g.Granularity = granularity;

	return g;
}

static const gdt_entry gdt[] = {
	make_gdt_entry(0, 0, 0, 0, false, false, false, false, false), // null descriptor
	make_gdt_entry(0, 0xfffff, 0x1a, 0, true, false, false, true, true), // kernel-mode code
	make_gdt_entry(0, 0xfffff, 0x12, 0, true, false, false, true, true) // kernel-mode data
};

struct gdt_desc {
	uint16_t size;
	const gdt_entry* ptr;
} __attribute__((packed));

gdt_desc gdt_descriptor{ sizeof(gdt) - 1, gdt };

extern "C" void enter_protected_mode(); // pm.s

extern "C"
void real_mode_main() {
	// FIXME - check A20

	enter_protected_mode();
}
