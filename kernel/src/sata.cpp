#include "kernel.h"
#include "pci.h"
#include "sata.h"
#include "unique_handle.h"

using namespace std;

static const uint8_t sata_interrupt = 0x22;
static bool interrupt_added = false;

void sata_device::read_test() {
    port.is = 0xffffffff; // clear interrupt bits

    uint8_t slot = 0; // FIXME

    volatile auto& cmd = *(HBA_CMD_HEADER*)(clb + (slot * sizeof(HBA_CMD_HEADER)));

    cmd.cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmd.w = 0; // read;
    cmd.prdtl = 1; // no. of PRDT entries

    print("cmd.ctba = ");
    print_hex(cmd.ctba);
    print("\n");

    auto& cmdtbl = *cmd_tables[slot];

    memset(&cmdtbl, 0, offsetof(HBA_CMD_TBL, prdt_entry) + (cmd.prdtl * sizeof(HBA_PRDT_ENTRY)));

    cmdtbl.prdt_entry[0].dba = 0; // FIXME - buffer
    cmdtbl.prdt_entry[0].dbau = 0; // FIXME - buffer
    cmdtbl.prdt_entry[0].dbc = 511; // size
    cmdtbl.prdt_entry[0].i = 1; // interrupt

    volatile auto& cmdfis = *(FIS_REG_H2D*)&cmdtbl.cfis;

    cmdfis.fis_type = FIS_TYPE::REG_H2D;
    cmdfis.c = 1; // command
    cmdfis.command = ata_command::READ_DMA_EX;

    uint64_t lba = 128; // FIXME - offset in sectors
    uint64_t size = 1; // FIXME - number of sectors

    cmdfis.lba0 = (uint8_t)lba;
    cmdfis.lba1 = (uint8_t)(lba >> 8);
    cmdfis.lba2 = (uint8_t)(lba >> 16);
    cmdfis.device = 0x40; // LBA mode

    cmdfis.lba3 = (uint8_t)(lba >> 24);
    cmdfis.lba4 = (uint8_t)(lba >> 32);
    cmdfis.lba5 = (uint8_t)(lba >> 40);

    cmdfis.countl = (uint8_t)size;
    cmdfis.counth = (uint8_t)(size >> 8);

    while (port.tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) {
    }

    port.ci = 1 << slot;

    while (true) {
        if (!(port.ci & (1 << slot)))
            break;

        if (port.is & HBA_PxIS_TFES) {
            print("Error reading disk\n");
            return;
        }
    }

    if (port.is & HBA_PxIS_TFES) {
        print("Error reading disk\n");
        return;
    }
}

bool sata_device::identify() {
    auto pa = allocate_page();
    if (!pa) {
        print("allocate_page failed\n");
        return false;
    }

    auto va = allocate_va(1);
    if (!va) {
        print("allocate_va failed\n");
        free_page(pa);
        return false;
    }

    if (!map_page(pa, va, mapping_type::read_write)) {
        print("map_page failed\n");
        free_page(pa);
        free_va(va, 1);
        return false;
    }

    port.is = 0; // clear interrupt bits

    uint8_t slot = 0; // FIXME

    volatile auto& cmd = *(HBA_CMD_HEADER*)(clb + (slot * sizeof(HBA_CMD_HEADER)));

    cmd.cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmd.w = 0; // read;
    cmd.prdtl = 1; // no. of PRDT entries

    auto& cmdtbl = *cmd_tables[slot];

    memset(&cmdtbl, 0, offsetof(HBA_CMD_TBL, prdt_entry) + (cmd.prdtl * sizeof(HBA_PRDT_ENTRY)));

    cmdtbl.prdt_entry[0].dba = (uint32_t)pa;
    cmdtbl.prdt_entry[0].dbau = (uint32_t)(pa >> 32);
    cmdtbl.prdt_entry[0].dbc = PAGE_SIZE - 1;
    cmdtbl.prdt_entry[0].i = 1; // interrupt

    volatile auto& cmdfis = *(FIS_REG_H2D*)&cmdtbl.cfis;

    cmdfis.fis_type = FIS_TYPE::REG_H2D;
    cmdfis.c = 1; // command
    cmdfis.command = ata_command::IDENTIFY;

    while (port.tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) {
    }

    port.ci = 1 << slot;

    while (true) {
        if (!(port.ci & (1 << slot)))
            break;

        if (port.is & HBA_PxIS_TFES) {
            print("Error sending IDENTIFY DEVICE command to disk\n");
            return false;
        }
    }

    if (port.is & HBA_PxIS_TFES) {
        print("Error sending IDENTIFY DEVICE command to disk\n");
        return false;
    }

    memcpy(&identify_block, (void*)va, sizeof(ata_identify_device_block));

    unmap_page(va);

    free_va(va, 1);
    free_page(pa);

    return true;
}

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

    port.ie = 0xfdc000ff; // enable all interrupts

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
        cmd_tables[i] = (HBA_CMD_TBL*)(va3 + (i * sizeof(HBA_CMD_TBL)));
    }

    identify();
//     read_test();
}

static void init_sata_device(void* ctx) {
    auto dev = new sata_device(*(volatile HBA_PORT*)ctx);

    // FIXME - add dev to list if successfully initialized
    // FIXME - add volume objects
    // FIXME - partitions (GPT and MBR)
}

extern "C"
void sata_isr_func() {
    print("SATA\n");
}

static constexpr uint32_t apic_msi_address(uint8_t processor, bool rh, bool dm) {
    return 0xfee00000 | (processor << 12) | (rh ? 0x8 : 0x0) | (dm ? 0x4 : 0x0);
}

void init_sata(const pci_device& dev) {
    auto abarmem = dev.bar[5];

    if (dev.msi.empty()) {
        print("MSI not available for SATA device\n");
        return;
    }

    if (abarmem.empty()) {
        print("ABAR not set for SATA device\n");
        return;
    }

    if (abarmem.size() < offsetof(HBA_MEM, ports)) {
        print("ABAR was shorter than expected\n");
        return;
    }

    if (!interrupt_added) {
        add_idt_entry(sata_interrupt, (void*)sata_isr, false, 0);
        interrupt_added = true;
    }

    auto& msi = *(volatile pci_msi*)dev.msi.data();

    msi.message_address_low = apic_msi_address(0, false, false); // FIXME - spread out amongst processors

    if (msi.message_control & 0x80) { // 64-bit
        msi.v64.message_address_high = 0;
        msi.v64.message_data = sata_interrupt;
    } else
        msi.v32.message_data = sata_interrupt;

    msi.message_control = msi.message_control | 1; // enable

    auto volatile& abar = *(HBA_MEM*)abarmem.data();

    // FIXME - check upper bit of abar.ghc for whether AHCI or IDE
    abar.ghc = abar.ghc | 0x2; // enable interrupts

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
