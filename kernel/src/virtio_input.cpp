#include <stdint.h>

import print;
import pci;
import virtio;
import list;
import module_init;

using namespace std;

static const uint64_t SUPPORTED_FEATURES = 0;

static const uint16_t PCI_VENDOR_ID_REDHAT = 0x1af4;

static void found_virtio_pci_input(pci_device& dev);

static pci_recognizer mod_pci_ids[] = {
    { {}, PCI_VENDOR_ID_REDHAT, 0x1052, true, found_virtio_pci_input },
};

static bool found_virtio_mmio_input(virtio_mmio_device& v);

static virtio_mmio_recognizer mod_virtio_mmio_types = {
    {}, 18, SUPPORTED_FEATURES, 2, found_virtio_mmio_input
};

static void init_virtio_pci() {
    for (unsigned int i = 0; i < sizeof(mod_pci_ids) / sizeof(*mod_pci_ids); i++) {
        insert_tail_list(pci_ids, mod_pci_ids[i].le);
    }

    insert_tail_list(virtio_mmio_types, mod_virtio_mmio_types.le);
}

static module_init module_init_virtio_input(init_virtio_pci);

static void found_virtio_pci_input(pci_device& dev) {
    print("FIXME - found_virtio_pci_input\n");
}

static bool found_virtio_mmio_input(virtio_mmio_device& dev) {
    print("FIXME - found_virtio_mmio_input\n");

    return true;
}
