module;

#include "kernel.h"
#include "pci.h"

export module nvme;

import print;

export void init_nvme(const pci_device& dev) {
    print("FIXME - init_nvme\n"); // FIXME
}
