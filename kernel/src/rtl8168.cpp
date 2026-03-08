#include <stdint.h>

import print;
import pci;
import list;
import module_init;

static const uint16_t PCI_VENDOR_ID_REALTEK = 0x10ec;

void found_rtl8168(pci_device& dev);

static pci_recognizer mod_pci_ids[] = {
    { {}, PCI_VENDOR_ID_REALTEK, 0x8168, true, found_rtl8168 },
};

static void init_rtl8168() {
    for (unsigned int i = 0; i < sizeof(mod_pci_ids) / sizeof(*mod_pci_ids); i++) {
        insert_tail_list(pci_ids, mod_pci_ids[i].le);
    }
}

static module_init module_init_rtl8168(init_rtl8168);

void found_rtl8168(pci_device& dev) {
    print("FIXME - found_rtl8168\n"); // FIXME
}
