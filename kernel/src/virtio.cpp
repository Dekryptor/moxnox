module;

#include "kernel.h"
#include "pci.h"

export module virtio;

import print;
import bsod;
import pci;

static const uint16_t PCI_VENDOR_ID_REDHAT = 0x1af4;

export void found_virtio_pci(pci_device& dev);

static const struct {
    uint16_t vendor_id;
    uint16_t device_id;
    void (*func)(pci_device&);
} mod_pci_ids[] = {
    { PCI_VENDOR_ID_REDHAT, 0x1003, found_virtio_pci },
    { PCI_VENDOR_ID_REDHAT, 0x1043, found_virtio_pci },
    { 0x0000, 0x0000, nullptr }
};

static void init_virtio_pci() {
    for (unsigned int i = 0; mod_pci_ids[i].vendor_id != 0x0000; i++) {
        auto ent = new pci_recognizer;
        if (!ent)
            bsod("new failed in initialization");

        ent->vendor_id = mod_pci_ids[i].vendor_id;
        ent->device_id = mod_pci_ids[i].device_id;
        ent->func = mod_pci_ids[i].func;

        insert_tail_list(pci_ids, ent->le);
    }
}

static module_init init(init_virtio_pci);

void found_virtio_pci(pci_device& dev) {
    print("FIXME: found_virtio_pci\n");
}
