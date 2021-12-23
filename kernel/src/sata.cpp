#include "kernel.h"
#include "pci.h"
#include "sata.h"
#include "unique_handle.h"

using namespace std;

sata_device::sata_device(volatile HBA_PORT& port) : port(port) {
    uint64_t clb_pa, fb_pa;

    clb_pa = ((uint64_t)port.clbu << 32) | port.clb;
    fb_pa = ((uint64_t)port.fbu << 32) | port.fb;

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

    clb = va1 | (clb_pa & (PAGE_SIZE - 1));
    fb = va2 | (fb_pa & (PAGE_SIZE - 1));

    unsigned int cmdtbl_pages = bytes_to_pages(SATA_NUM_CMD_TABLES * sizeof(HBA_CMD_TBL));

    auto va3 = allocate_va(cmdtbl_pages);
    if (!va3) {
        print("allocate_va failed\n");
        unmap_page(va2);
        unmap_page(va1);
        free_va(va2, 1);
        free_va(va1, 1);
    }

    for (unsigned int i = 0; i < cmdtbl_pages; i++) {
        auto pa = allocate_page();
        if (!pa) {
            print("allocate_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                auto pa = va_to_pa(va3 + (j << PAGE_SHIFT));

                unmap_page(va3 + (j << PAGE_SHIFT));
                free_page(pa);
            }

            free_va(va3, cmdtbl_pages);
            unmap_page(va2);
            unmap_page(va1);
            free_va(va2, 1);
            free_va(va1, 1);
            return;
        }

        auto cmd_tbls = (HBA_CMD_HEADER*)clb;

        for (unsigned int j = 0; j < PAGE_SIZE / sizeof(HBA_CMD_TBL); j++) {
            volatile auto& tbl = cmd_tbls[(i * PAGE_SIZE / sizeof(HBA_CMD_TBL)) + j];

            tbl.ctba = (uint32_t)(pa + (j * sizeof(HBA_CMD_TBL)));
            tbl.ctbau = (uint32_t)(pa >> 32);
        }

        if (!map_page(pa, va3 + (i << PAGE_SHIFT), mapping_type::no_cache)) {
            print("map_page failed\n");

            free_page(pa);

            for (unsigned int j = 0; j < i; j++) {
                auto pa = va_to_pa(va3 + (j << PAGE_SHIFT));

                unmap_page(va3 + (j << PAGE_SHIFT));
                free_page(pa);
            }

            free_va(va3, cmdtbl_pages);
            unmap_page(va2);
            unmap_page(va1);
            free_va(va2, 1);
            free_va(va1, 1);
            return;
        }
    }

    memset((void*)va3, 0, cmdtbl_pages << PAGE_SHIFT);

    for (unsigned int i = 0; i < SATA_NUM_CMD_TABLES; i++) {
        cmd_tables[i] = (volatile HBA_CMD_TBL*)(va3 + (i * sizeof(HBA_CMD_TBL)));
    }

    print("clb = ");
    print_hex(clb);
    print(", fb = ");
    print_hex(fb);
    print("\n");
}

static void init_sata_device(void* ctx) {
    sata_device dev(*(volatile HBA_PORT*)ctx);

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
