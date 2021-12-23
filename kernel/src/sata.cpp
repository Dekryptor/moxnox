#include "kernel.h"
#include "pci.h"
#include "sata.h"

using namespace std;

void init_sata(const pci_device& dev) {
    auto abarmem = dev.bar[5];

    if (abarmem.empty()) {
        print("ABAR not set for SATA device\n");
        return;
    }

    if (abarmem.size() < offsetof(HBA_MEM, ports)) {
        print("ABAR was shorter than expected\n");
        return;
    }

    auto volatile& abar = *(HBA_MEM*)abarmem.data();

    auto ports = span<volatile HBA_PORT>((HBA_PORT*)abar.ports, (abarmem.size() - offsetof(HBA_MEM, ports)) / sizeof(HBA_PORT));
    uint32_t mask = abar.pi;

    for (const auto& p : ports) {
        if (!(mask & 1)) {
            mask >>= 1;
            continue;
        }

        // FIXME - SATA_SIG_ATAPI (CD drives etc.)

        if (p.sig == SATA_SIG_ATA) {
            // FIXME
            print("FIXME - HDD\n");
        }

        mask >>= 1;
    }
}
