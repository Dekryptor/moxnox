module;

#include "kernel.h"

export module xhci;

import pci;
import print;

export
extern "C++"
void init_xhci(const pci_device& dev) {
    print("FIXME - init_xhci\n");
}
