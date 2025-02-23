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

extern "C" const gdt_entry gdt;

struct gdt_desc {
	uint16_t size;
	const gdt_entry* ptr;
} __attribute__((packed));

static_assert(sizeof(struct gdt_desc) == 6);

gdt_desc gdt_descriptor{ (3 * sizeof(gdt_entry)) - 1, &gdt };

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
