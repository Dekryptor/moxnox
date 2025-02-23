// Clang doesn't have stdint.h in 16-bit mode(?)
using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;

static_assert(sizeof(uint8_t) == 1);
static_assert(sizeof(uint16_t) == 2);
static_assert(sizeof(uint32_t) == 4);

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

static_assert(sizeof(struct gdt_desc) == 6);

gdt_desc gdt_descriptor{ sizeof(gdt) - 1, gdt };

extern "C" void enter_protected_mode(); // pm.s

// boot.S
extern "C" bool vbe_get_info();
extern "C" void vbe_get_edid();
extern "C" void vbe_get_modes();
extern "C" void vbe_choose_mode();
extern "C" void parse_cmdline();

extern "C"
__attribute__((section(".rm_text")))
void real_mode_main() {
	// FIXME - check A20

	parse_cmdline();

	if (vbe_get_info()) {
		vbe_get_edid();
		vbe_get_modes();
		vbe_choose_mode();
	}

	enter_protected_mode();
}
