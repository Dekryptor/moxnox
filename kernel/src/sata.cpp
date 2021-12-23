#include "kernel.h"
#include "pci.h"
#include "sata.h"
#include "unique_handle.h"

using namespace std;

static void init_sata_device(void* ctx) {
    auto& p = *(volatile HBA_PORT*)ctx;
    uint64_t clb_pa, fb_pa;

    clb_pa = ((uint64_t)p.clbu << 32) | p.clb;
    fb_pa = ((uint64_t)p.fbu << 32) | p.fb;

    auto va1 = allocate_va(1);
    if (!va1) {
        print("allocate_va failed\n");
        return;
    }

    auto va2 = allocate_va(1);
    if (!va2) {
        print("allocate_va failed\n");
        free_va(va1, 1);
        return;
    }

    if (!map_page(clb_pa & ~(PAGE_SIZE - 1), va1, mapping_type::no_cache)) {
        print("map_page failed\n");
        free_va(va2, 1);
        free_va(va1, 1);
        return;
    }

    if (!map_page(fb_pa & ~(PAGE_SIZE - 1), va2, mapping_type::no_cache)) {
        print("map_page failed\n");
        unmap_page(va1);
        free_va(va2, 1);
        free_va(va1, 1);
        return;
    }

    // FIXME - can remap physical addresses, if BIOS hasn't set them up right?

    uint64_t clb = va1 | (clb_pa & (PAGE_SIZE - 1));
    uint64_t fb = va2 | (fb_pa & (PAGE_SIZE - 1));

    print("clb = ");
    print_hex(clb);
    print(", fb = ");
    print_hex(fb);
    print("\n");

    // FIXME
}

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

        if (p.sig == SATA_SIG_ATA)
            add_kernel_thread(init_sata_device, (void*)&p);

        mask >>= 1;
    }
}
