module;

export module pl011;

import module_init;
import acpi;

static void found_pl011(const acpi_device& d);

static acpi_recognizer mod_pnp_hids[] = {
    { {}, "ARMH0011", found_pl011, nullptr },
};

static void init_acpi_hids() {
    for (unsigned int i = 0; i < sizeof(mod_pnp_hids) / sizeof(*mod_pnp_hids); i++) {
        insert_tail_list(pnp_hids, mod_pnp_hids[i].le);
    }
}

static module_init module_init_keyboard(init_acpi_hids);

static void found_pl011(const acpi_device& d) {
    // FIXME
}
