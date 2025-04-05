module;

#include "kernel.h"
#include "pci.h"

export module virtio;

import print;

export void found_virtio_pci(pci_device& dev) {
    print("FIXME: found_virtio_pci\n");
}
