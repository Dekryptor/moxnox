module;

#include <stdint.h>

import pci;
import module_init;
import list;
import print;

export module sdhci;

using namespace std;

static bool init_sdhci(const pci_device& dev);

static pci_class_recognizer mod_pci_class = {
    {}, pci_class::base_system_peripheral, (uint8_t)pci_base_system_peripheral::sdhci, true, init_sdhci
};

static module_init module_init_xhci([]() {
    insert_tail_list(pci_classes, mod_pci_class.le);
});

static bool init_sdhci(const pci_device& dev) {
    print("FIXME - init_sdhci\n"); // FIXME

    return false;
}
