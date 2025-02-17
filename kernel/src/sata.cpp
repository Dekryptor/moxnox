#include "kernel.h"
#include "list.h"
#include "pci.h"
#include "sata.h"
#include "spinlock.h"
#include "mem.h"

import format;

using namespace std;

static const uint8_t sata_interrupt = 0x22;
static bool interrupt_added = false;

static spinlock sata_list_lock;
static list_entry sata_list = { &sata_list, &sata_list };

static constexpr uint32_t SATA_IS_CPDS = 1 << 31;
static constexpr uint32_t SATA_IS_TFES = 1 << 30;
static constexpr uint32_t SATA_IS_HBFS = 1 << 29;
static constexpr uint32_t SATA_IS_HBDS = 1 << 28;
static constexpr uint32_t SATA_IS_IFS  = 1 << 27;
static constexpr uint32_t SATA_IS_INFS = 1 << 26;
static constexpr uint32_t SATA_IS_OFS  = 1 << 24;
static constexpr uint32_t SATA_IS_IPMS = 1 << 23;
static constexpr uint32_t SATA_IS_PRCS = 1 << 22;
static constexpr uint32_t SATA_IS_PMPS = 1 << 7;
static constexpr uint32_t SATA_IS_PCS  = 1 << 6;
static constexpr uint32_t SATA_IS_DPS  = 1 << 5;
static constexpr uint32_t SATA_IS_UFS  = 1 << 4;
static constexpr uint32_t SATA_IS_SDBS = 1 << 3;
static constexpr uint32_t SATA_IS_DSS  = 1 << 2;
static constexpr uint32_t SATA_IS_PSS  = 1 << 1;
static constexpr uint32_t SATA_IS_DHRS = 1 << 0;

atomic<unsigned int> num_sata_devices = 0;

#if 0
size_t sata_device::read_uncached(span<uint8_t> data, uint64_t offset) {
    size_t ret = 0;

    if (offset & (sector_size - 1)) {
        print("Invalid offset\n");
        return 0;
    }

    if (data.size() & (sector_size - 1)) {
        print("Invalid length\n");
        return 0;
    }

    // FIXME - trim data length if would overflow device

    // FIXME - proper error handling
    // FIXME - asynchronous mode

    while (!data.empty()) {
        port.is = 0xffffffff; // clear interrupt bits

        uint8_t slot = get_slot();

        volatile auto& cmd = *(HBA_CMD_HEADER*)(clb + (slot * sizeof(HBA_CMD_HEADER)));

        cmd.cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
        cmd.w = 0; // read

        auto& cmdtbl = *cmd_tables[slot];

        memset(&cmdtbl, 0, offsetof(HBA_CMD_TBL, prdt_entry) + (cmd.prdtl * sizeof(HBA_PRDT_ENTRY)));

        size_t to_read = 0;

        auto buf = data;
        uint16_t num_entries = 0;

        for (auto& e : cmdtbl.prdt_entry) {
            uint32_t left_in_page = ((uintptr_t)buf.data() & (PAGE_SIZE - 1)) ? (PAGE_SIZE - ((uintptr_t)buf.data() & (PAGE_SIZE - 1))) : PAGE_SIZE;
            uint32_t r = min(left_in_page, (uint32_t)buf.size());
            uint64_t pa = va_to_pa((uintptr_t)buf.data() & ~(PAGE_SIZE - 1)) + ((uintptr_t)buf.data() & (PAGE_SIZE - 1));

            // FIXME - make sure device supports 64-bit PAs

            e.dba = (uint32_t)pa;
            e.dbau = (uint32_t)(pa >> 32);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            e.dbc = (unsigned int)(r - 1);
#pragma GCC diagnostic pop
            e.i = 0; // interrupt

            buf = buf.subspan(r);
            num_entries++;
            to_read += r;

            if (buf.empty())
                break;
        }

        cmdtbl.prdt_entry[num_entries - 1].i = 1;

        cmd.prdtl = num_entries;

        volatile auto& cmdfis = *(FIS_REG_H2D*)&cmdtbl.cfis;

        cmdfis.fis_type = FIS_TYPE::REG_H2D;
        cmdfis.c = 1; // command
        cmdfis.command = ata_command::READ_DMA_EX;

        uint64_t lba = offset / sector_size;
        uint64_t size = to_read / sector_size;

        cmdfis.lba0 = (uint8_t)lba;
        cmdfis.lba1 = (uint8_t)(lba >> 8);
        cmdfis.lba2 = (uint8_t)(lba >> 16);
        cmdfis.device = 0x40; // LBA mode

        cmdfis.lba3 = (uint8_t)(lba >> 24);
        cmdfis.lba4 = (uint8_t)(lba >> 32);
        cmdfis.lba5 = (uint8_t)(lba >> 40);

        cmdfis.countl = (uint8_t)size;
        cmdfis.counth = (uint8_t)(size >> 8);

        pending_slots |= 1 << slot;

        {
            lock_guard lock(ci_lock);
            port.ci |= 1 << slot;
        }

        wait_for_object(events[slot].get(), false);

        if (port.is & HBA_PxIS_TFES) {
            print("Error reading disk\n");
            release_slot(slot);
            return 0;
        }

        offset += to_read;
        data = data.subspan(to_read);
        ret += to_read;

        release_slot(slot);
    }

    return ret;
}
#endif

uint64_t sata_device::read_into_cache(uint64_t page_num, uint64_t num_pages) {
    // FIXME - proper error handling
    // FIXME - asynchronous mode

    port.is = 0xffffffff; // clear interrupt bits

    uint8_t slot = get_slot();

    volatile auto& cmd = *(HBA_CMD_HEADER*)(clb + (slot * sizeof(HBA_CMD_HEADER)));

    cmd.cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmd.w = 0; // read

    auto& cmdtbl = *cmd_tables[slot];

    memset(&cmdtbl, 0, offsetof(HBA_CMD_TBL, prdt_entry) + (cmd.prdtl * sizeof(HBA_PRDT_ENTRY)));

    if (num_pages > NUM_PRDT_ENTRIES)
        num_pages = NUM_PRDT_ENTRIES;

    {
        lock_guard lg(cache.cache_lock);

        auto* p = cache.find_page_cache_entry(page_num);

        for (unsigned int i = 0; i < num_pages; i++) {
            auto& e = cmdtbl.prdt_entry[i];

            if (i != 0)
                p = &containing_record<page_cache_entry>(p->le.next);

            // FIXME - make sure device supports 64-bit PAs

            e.dba = (uint32_t)p->phys_address;
            e.dbau = (uint32_t)(p->phys_address >> 32);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            e.dbc = (unsigned int)(PAGE_SIZE - 1); // FIXME - what if device not whole number of pages?
#pragma GCC diagnostic pop
            e.i = 0; // interrupt
        }
    }

    cmdtbl.prdt_entry[num_pages - 1].i = 1;

    cmd.prdtl = (uint16_t)num_pages;

    volatile auto& cmdfis = *(FIS_REG_H2D*)&cmdtbl.cfis;

    cmdfis.fis_type = FIS_TYPE::REG_H2D;
    cmdfis.c = 1; // command
    cmdfis.command = ata_command::READ_DMA_EX;

    uint64_t lba = page_num * (PAGE_SIZE / sector_size);
    auto num_sectors = (uint16_t)(num_pages * (PAGE_SIZE / sector_size));

    cmdfis.lba0 = (uint8_t)lba;
    cmdfis.lba1 = (uint8_t)(lba >> 8);
    cmdfis.lba2 = (uint8_t)(lba >> 16);
    cmdfis.device = 0x40; // LBA mode

    cmdfis.lba3 = (uint8_t)(lba >> 24);
    cmdfis.lba4 = (uint8_t)(lba >> 32);
    cmdfis.lba5 = (uint8_t)(lba >> 40);

    cmdfis.countl = (uint8_t)num_sectors;
    cmdfis.counth = (uint8_t)(num_sectors >> 8);

    pending_slots |= 1 << slot;

    {
        lock_guard lock(ci_lock);
        port.ci |= 1 << slot;
    }

    // FIXME - should this be uninterruptible?
    auto status = wait_for_object(events[slot].get(), true);

    if (status == nt_status::ALERTED) {
        release_slot(slot);
        return 0;
    }

    if (port.is & HBA_PxIS_TFES) {
        print("Error reading disk\n");
        release_slot(slot);
        return 0;
    }

    release_slot(slot);

    return num_pages;
}

NTSTATUS sata_device::read(span<uint8_t> data, uint64_t offset, uint64_t& read_bytes) {
    // FIXME - clear page cache in destructor

    // FIXME - return error if offset past end of device
    // FIXME - truncate data buffer if we would read past end of device

    // FIXME - return errors properly
    read_bytes = cache.read(data, offset);
    return nt_status::SUCCESS;
}

uint8_t sata_device::get_slot() {
    wait_for_object(slot_semaphore.get(), false);

    lock_guard l(slot_lock);

    uint32_t mask = 1;
    uint8_t c = 0;

    while (true) {
        if (!(allocated_slots & mask)) {
            allocated_slots |= mask;
            return c;
        }

        mask <<= 1;
        c++;
    }
}

void sata_device::release_slot(uint8_t slot) {
    {
        lock_guard l(slot_lock);

        allocated_slots &= ~(1 << (uint32_t)slot);
    }

    release_semaphore(static_cast<semaphore&>(slot_semaphore.get()));
}

bool sata_device::identify() {
    auto pa = allocate_page();
    if (!pa) {
        print("allocate_page failed\n");
        return false;
    }

    auto va = phys_address(pa);

    port.is = 0xffffffff; // clear interrupt bits

    auto slot = get_slot();

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

    pending_slots |= 1 << slot;

    port.ci = port.ci | (1 << slot);

    // IDENTIFY doesn't necessarily lead to an interrupt on real hardware

    while (port.ci & 1 << slot) { }

    pending_slots &= ~(1 << slot);

    reset_event(static_cast<event&>(events[slot].get()));

    if (port.is & HBA_PxIS_TFES) {
        print("Error sending IDENTIFY DEVICE command to disk\n");
        release_slot(slot);
        free_page(pa);
        return false;
    }

    memcpy(&identify_block, (void*)va, sizeof(ata_identify_device_block));

    release_slot(slot);

    free_page(pa);

    return true;
}

sata_device::sata_device(volatile HBA_PORT& port) : port(port) {
    port.cmd = port.cmd & ~HBA_PxCMD_ST;
    port.cmd = port.cmd & ~HBA_PxCMD_FRE;

    while (port.cmd & (HBA_PxCMD_FR | HBA_PxCMD_CR)) { }

    auto clb_pa = allocate_page();
    if (!clb_pa) {
        print("allocate_page failed\n");
        return;
    }

    auto fb_pa = allocate_page();
    if (!fb_pa) {
        print("allocate_page failed\n");
        free_page(clb_pa);
        return;
    }

    clb = allocate_va(1);
    if (!clb) {
        print("allocate_va failed\n");
        free_page(fb_pa);
        free_page(clb_pa);
        return;
    }

    fb = allocate_va(1);
    if (!fb) {
        print("allocate_va failed\n");
        free_va(clb, 1);
        free_page(fb_pa);
        free_page(clb_pa);
        return;
    }

    if (!map_page(clb_pa, clb, mapping_type::no_cache)) {
        print("map_page failed\n");
        free_va(fb, 1);
        free_va(clb, 1);
        free_page(fb_pa);
        free_page(clb_pa);
        return;
    }

    if (!map_page(fb_pa, fb, mapping_type::no_cache)) {
        print("map_page failed\n");
        unmap_page(clb);
        free_va(fb, 1);
        free_va(clb, 1);
        free_page(fb_pa);
        free_page(clb_pa);
        return;
    }

    memset((void*)clb, 0, PAGE_SIZE);
    memset((void*)fb, 0, PAGE_SIZE);

    port.is = 0xffffffff;

    // enable all interrupts
    port.ie = SATA_IS_CPDS | SATA_IS_TFES | SATA_IS_HBFS | SATA_IS_HBDS |
              SATA_IS_IFS | SATA_IS_INFS | SATA_IS_OFS | SATA_IS_IPMS |
              SATA_IS_PRCS | SATA_IS_PMPS | SATA_IS_PCS | SATA_IS_DPS |
              SATA_IS_UFS | SATA_IS_SDBS | SATA_IS_DSS | SATA_IS_PSS |
              SATA_IS_DHRS;

    unsigned int cmdtbl_pages = bytes_to_pages(SATA_NUM_CMD_TABLES * sizeof(HBA_CMD_TBL));

    auto va3 = allocate_va(cmdtbl_pages);
    if (!va3) {
        print("allocate_va failed\n");
        unmap_page(fb);
        unmap_page(clb);
        free_va(fb, 1);
        free_va(clb, 1);
        free_page(fb_pa);
        free_page(clb_pa);
        return;
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
            unmap_page(fb);
            unmap_page(clb);
            free_va(fb, 1);
            free_va(clb, 1);
            free_page(fb_pa);
            free_page(clb_pa);
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
            unmap_page(fb);
            unmap_page(clb);
            free_va(fb, 1);
            free_va(clb, 1);
            free_page(fb_pa);
            free_page(clb_pa);
            return;
        }
    }

    port.clb = clb_pa & 0xffffffff;
    port.clbu = (uint32_t)(clb_pa >> 32);
    port.fb = fb_pa & 0xffffffff;
    port.fbu = (uint32_t)(fb_pa >> 32);

    memset((void*)va3, 0, cmdtbl_pages << PAGE_SHIFT);

    for (unsigned int i = 0; i < SATA_NUM_CMD_TABLES; i++) {
        cmd_tables[i] = (HBA_CMD_TBL*)(va3 + (i * sizeof(HBA_CMD_TBL)));
    }

    for (auto& ev : events) {
        ev = create_event(true);
    }

    slot_semaphore = create_semaphore(SATA_NUM_CMD_TABLES);

    while (port.cmd & HBA_PxCMD_CR) { }

    port.cmd = port.cmd | HBA_PxCMD_FRE;
    port.cmd = port.cmd | HBA_PxCMD_ST;

    valid = true;
}

static void fix_ata_string(span<const uint16_t> in, span<char> out) {
    auto it = out.begin();

    for (auto u : in) {
        if (it == out.end())
            break;

        *it = (char)(u >> 8);
        it++;

        if (it == out.end())
            break;

        *it = (char)(u & 0xff);
        it++;
    }
}

static void init_sata_device(void* ctx, void* ctx2) {
    auto pdev = new sata_device(*(volatile HBA_PORT*)ctx);
    auto& sc = *(sata_card*)ctx2;
    auto& dev = *pdev;

    if (!dev.valid) {
        delete &dev;

        if (--sc.init_left == 0)
            set_event(static_cast<event&>(sc.init_event.get()));

        return;
    }

    {
        lock_guard lock(sc.port_list_lock);
        insert_tail_list(sc.port_list, dev.le);
    }

    // FIXME - return if IDENTIFY fails, and remove from list
    dev.identify();

    fix_ata_string(span((uint16_t*)dev.identify_block.model_number, sizeof(dev.identify_block.model_number) / sizeof(uint16_t)), dev.model_number_buf);

    dev.model_number = dev.model_number_buf;
    while (!dev.model_number.empty() && (dev.model_number.back() == 0 || dev.model_number.back() == ' ')) {
        dev.model_number.remove_suffix(1);
    }

    print("Found disk: {}\n", dev.model_number);

    dev.sector_size = 512; // FIXME - get from identify block
    dev.size = dev.identify_block.user_addressable_sectors * dev.sector_size;
    dev.dev_num = num_sata_devices.fetch_add(1);

    // FIXME - make sure SATA devices get numbered consistently

    if (dev.dev_num < 26) {
        char name[4];

        name[0] = 's';
        name[1] = 'd';
        name[2] = (char)(dev.dev_num + 'a');
        name[3] = 0;

        if (!name_object(dev, name)) {
            print("name_object failed\n");
            goto end;
        }

        dev.volume_name_len = strlen(name);
        dev.volume_name = (char*)malloc(dev.volume_name_len);
        if (!dev.volume_name) {
            print("malloc failed\n");
            goto end;
        }

        memcpy(dev.volume_name, name, dev.volume_name_len);
    }

    dev.probe();

    dev.release();

end:
    if (--sc.init_left == 0)
        set_event(static_cast<event&>(sc.init_event.get()));
}

void sata_device::isr() {
    auto is = port.is;

    port.is = is;

    if (!(is & SATA_IS_DHRS))
        return;

    // D2H FIS received
    auto ci = port.ci;
    auto signalled = pending_slots & ~ci;

    for (unsigned int i = 0; i < SATA_NUM_CMD_TABLES; i++) {
        if (signalled & (1 << i)) {
            set_event(static_cast<event&>(events[i].get()));
            pending_slots &= ~(1 << i);
        }
    }
}

void sata_card::isr() {
    auto abar_is = abar->is;

    abar->is = abar_is;

    if (abar_is == 0)
        return;

    // FIXME - abar->is indicates which sata device we should look at?

    lock_guard lock(port_list_lock);

    auto le = port_list.next;
    while (le != &port_list) {
        auto& dev = containing_record<sata_device>(le);

        dev.isr();

        le = le->next;
    }
}

extern "C"
void sata_isr_func() {
    lock_guard lock(sata_list_lock);

    auto le = sata_list.next;
    while (le != &sata_list) {
        auto& sc = containing_record<sata_card>(le);

        sc.isr();

        le = le->next;
    }
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

    auto sc = new sata_card(dev);
    if (!sc) {
        print("new failed\n");
        return;
    }

    if (!interrupt_added) {
        add_idt_entry(sata_interrupt, (void*)sata_isr, false, 0, false);
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

    sc->abar = (HBA_MEM*)abarmem.data();

    // FIXME - check upper bit of abar.ghc for whether AHCI or IDE
    sc->abar->ghc = sc->abar->ghc | 0x2; // enable interrupts

    auto ports_ptr = (volatile HBA_PORT*)sc->abar->ports;
    size_t ports_count = (abarmem.size() - offsetof(HBA_MEM, ports)) / sizeof(HBA_PORT);

    uint32_t mask = sc->abar->pi;

    for (size_t i = 0; i < ports_count; i++) {
        if (!(mask & 1)) {
            mask >>= 1;
            continue;
        }

        const auto& p = ports_ptr[i];

        // FIXME - SATA_SIG_ATAPI (CD drives etc.)

        if (p.sig == SATA_SIG_ATA)
            sc->init_left++;

        mask >>= 1;
    }

    if (sc->init_left == 0) {
        delete sc;
        return;
    }

    {
        lock_guard lock(sata_list_lock);
        insert_tail_list(sata_list, sc->le);
    }

    sc->init_event = create_event(true);

    mask = sc->abar->pi;

    for (size_t i = 0; i < ports_count; i++) {
        if (!(mask & 1)) {
            mask >>= 1;
            continue;
        }

        const auto& p = ports_ptr[i];

        // FIXME - SATA_SIG_ATAPI (CD drives etc.)

        if (p.sig == SATA_SIG_ATA)
            add_kernel_thread(init_sata_device, (void*)&p, sc);

        mask >>= 1;
    }

    wait_for_object(sc->init_event.get(), false);
}

size_t sata_device::flush_cache(uint64_t page_num, uint64_t num_pages) {
    // FIXME - cache lock?

    // FIXME - we're assuming here the pages are dirty, is that safe?

    // FIXME - proper error handling
    // FIXME - asynchronous mode

    port.is = 0xffffffff; // clear interrupt bits

    uint8_t slot = get_slot();

    volatile auto& cmd = *(HBA_CMD_HEADER*)(clb + (slot * sizeof(HBA_CMD_HEADER)));

    cmd.cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmd.w = 0; // read

    auto& cmdtbl = *cmd_tables[slot];

    memset(&cmdtbl, 0, offsetof(HBA_CMD_TBL, prdt_entry) + (cmd.prdtl * sizeof(HBA_PRDT_ENTRY)));

    if (num_pages > NUM_PRDT_ENTRIES)
        num_pages = NUM_PRDT_ENTRIES;

    {
        lock_guard lg(cache.cache_lock);

        auto* p = cache.find_page_cache_entry(page_num);

        for (unsigned int i = 0; i < num_pages; i++) {
            auto& e = cmdtbl.prdt_entry[i];

            if (i != 0)
                p = &containing_record<page_cache_entry>(p->le.next);

            assert(p->page_num == page_num + i);

            // FIXME - make sure device supports 64-bit PAs

            e.dba = (uint32_t)p->phys_address;
            e.dbau = (uint32_t)(p->phys_address >> 32);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            e.dbc = (unsigned int)(PAGE_SIZE - 1); // FIXME - what if device not whole number of pages?
#pragma GCC diagnostic pop
            e.i = 0; // interrupt
        }
    }

    cmdtbl.prdt_entry[num_pages - 1].i = 1;

    cmd.prdtl = (uint16_t)num_pages;

    volatile auto& cmdfis = *(FIS_REG_H2D*)&cmdtbl.cfis;

    cmdfis.fis_type = FIS_TYPE::REG_H2D;
    cmdfis.c = 1; // command
    cmdfis.command = ata_command::WRITE_DMA_EX;

    uint64_t lba = page_num * (PAGE_SIZE / sector_size);
    auto num_sectors = (uint16_t)(num_pages * (PAGE_SIZE / sector_size));

    cmdfis.lba0 = (uint8_t)lba;
    cmdfis.lba1 = (uint8_t)(lba >> 8);
    cmdfis.lba2 = (uint8_t)(lba >> 16);
    cmdfis.device = 0x40; // LBA mode

    cmdfis.lba3 = (uint8_t)(lba >> 24);
    cmdfis.lba4 = (uint8_t)(lba >> 32);
    cmdfis.lba5 = (uint8_t)(lba >> 40);

    cmdfis.countl = (uint8_t)num_sectors;
    cmdfis.counth = (uint8_t)(num_sectors >> 8);

    pending_slots |= 1 << slot;

    {
        lock_guard lock(ci_lock);
        port.ci |= 1 << slot;
    }

    wait_for_object(events[slot].get(), false);

    if (port.is & HBA_PxIS_TFES) {
        print("Error writing to disk\n");
        release_slot(slot);
        return 0;
    }

    release_slot(slot);

    // mark page cache entries as clean

    {
        lock_guard lg(cache.cache_lock);

        auto* p = cache.find_page_cache_entry(page_num);

        for (unsigned int i = 0; i < num_pages; i++) {
            if (i != 0)
                p = &containing_record<page_cache_entry>(p->le.next);

            assert(p->page_num == page_num + i);

            p->status = page_cache_status::clean;
        }
    }

    return num_pages;
}
