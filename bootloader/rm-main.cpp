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
