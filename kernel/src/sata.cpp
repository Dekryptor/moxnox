module;

#include <shared_mutex>
#include <span>
#include <string_view>
#include <mutex>
#include <atomic>
#include <string.h>
#include "tick_rate.h"

export module sata;

import nt;
import print;
import format;
import bsod;
import mem;
import config;
import list;
import interrupts;
import semaphore;
import event;
import volume;
import pci;
import timer;
import module_init;
import thread;
import page_cache;
import rwsem;
import scheduler;
import spinlock;
import handle;
import object_ref;

#include "sata.h"

using namespace std;

static spinlock sata_list_lock;
static list_entry sata_list = { &sata_list, &sata_list };

static constexpr uint32_t SATA_IS_CPDS = (uint32_t)(1 << 31);
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

static atomic<unsigned int> num_sata_devices = 0;

#define ONE_MS_TICKS (SYSTEM_TIME_RESOLUTION / 1000) // FIXME

static bool init_sata(const pci_device& dev);

static pci_class_recognizer mod_pci_class = {
    {}, pci_class::mass_storage, (uint8_t)pci_mass_storage::sata, false, init_sata
};

static module_init module_init_nvme([]() {
    insert_tail_list(pci_classes, mod_pci_class.le);
});

nt_status sata_device::dio_read(uint64_t page_num, uint64_t num_pages,
                                pfn_iterator& it, uint64_t& read_pages) {
    // FIXME - proper error handling
    // FIXME - asynchronous mode

    port.is = 0xffffffff; // clear interrupt bits

    uint8_t slot = get_slot();

    auto& cmd = *(HBA_CMD_HEADER*)(clb + (slot * sizeof(HBA_CMD_HEADER)));

    cmd.cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmd.w = 0; // read
    cmd.prdbc = 0;

    auto& cmdtbl = *cmd_tables[slot];

    memset(&cmdtbl, 0, offsetof(HBA_CMD_TBL, prdt_entry) + (cmd.prdtl * sizeof(HBA_PRDT_ENTRY)));

    if (num_pages > NUM_PRDT_ENTRIES)
        num_pages = NUM_PRDT_ENTRIES;

    for (unsigned int i = 0; i < num_pages; i++) {
        auto& e = cmdtbl.prdt_entry[i];

        if (i != 0)
            it++;

        // FIXME - make sure device supports 64-bit PAs

        e.dba = (uint32_t)*it;
        e.dbau = (uint32_t)(*it >> 32);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        e.dbc = (unsigned int)(PAGE_SIZE - 1); // FIXME - what if device not whole number of pages?
#pragma GCC diagnostic pop
        e.i = 0; // interrupt
    }

    cmdtbl.prdt_entry[num_pages - 1].i = 1;

    cmd.prdtl = (uint16_t)num_pages;

    auto& cmdfis = *(FIS_REG_H2D*)&cmdtbl.cfis;

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
    port.ci = 1 << slot;

    // FIXME - should this be uninterruptible?
    auto status = wait_for_object(events[slot].get(), true);

    if (status == nt_status::ALERTED || status == nt_status::THREAD_IS_TERMINATING) {
        release_slot(slot);
        read_pages = 0;
        return nt_status::SUCCESS;
    }

    if (port.is & HBA_PxIS_TFES) {
        print("Error reading disk\n");
        release_slot(slot);
        return nt_status::INTERNAL_ERROR; // FIXME - translate into nt_status properly
    }

    release_slot(slot);

    read_pages = num_pages;

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

        allocated_slots &= ~((uint32_t)1 << slot);
    }

    release_semaphore(static_cast<semaphore&>(slot_semaphore.get()));
}

bool sata_device::identify() {
    auto buf = malloc(sizeof(ata_identify_device_block));
    if (!buf) {
        print("malloc failed\n");
        return false;
    }

    port.is = 0xffffffff; // clear interrupt bits

    auto slot = get_slot();

    auto& cmd = *(HBA_CMD_HEADER*)(clb + (slot * sizeof(HBA_CMD_HEADER)));

    cmd.cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmd.w = 0; // read
    cmd.prdtl = 1; // no. of PRDT entries
    cmd.prdbc = 0;

    auto& cmdtbl = *cmd_tables[slot];

    memset(&cmdtbl, 0, offsetof(HBA_CMD_TBL, prdt_entry) + (cmd.prdtl * sizeof(HBA_PRDT_ENTRY)));

    auto pa = va_to_pa_phys(buf);

    cmdtbl.prdt_entry[0].dba = (uint32_t)pa;
    cmdtbl.prdt_entry[0].dbau = (uint32_t)(pa >> 32);
    cmdtbl.prdt_entry[0].dbc = sizeof(ata_identify_device_block) - 1;
    cmdtbl.prdt_entry[0].i = 1; // interrupt

    auto& cmdfis = *(FIS_REG_H2D*)&cmdtbl.cfis;

    cmdfis.fis_type = FIS_TYPE::REG_H2D;
    cmdfis.c = 1; // command
    cmdfis.command = ata_command::IDENTIFY;

    pending_slots |= 1 << slot;
    port.ci = 1 << slot;

    auto status = wait_for_object(events[slot].get(), true);
    if (!success(status)) {
        print("sata_device::identify: wait_for_object returned {}\n", status);
        pending_slots &= ~((uint32_t)1 << slot);
        release_slot(slot);
        free(buf);
        return false;
    }

    pending_slots &= ~((uint32_t)1 << slot);

    reset_event(static_cast<event&>(events[slot].get()));

    if (port.is & HBA_PxIS_TFES) {
        print("Error sending IDENTIFY DEVICE command to disk\n");
        release_slot(slot);
        free(buf);
        return false;
    }

    memcpy(&identify_block, buf, sizeof(ata_identify_device_block));

    release_slot(slot);

    free(buf);

    return true;
}

sata_device::sata_device(volatile HBA_PORT& port) : port(port) {
    auto clb_pa = allocate_page();
    if (!clb_pa) {
        print("allocate_page failed\n");
        return;
    }

    auto fb_pa = allocate_page();
    if (!fb_pa) {
        print("allocate_page failed\n");
        free_folio(clb_pa);
        return;
    }

    clb = allocate_va(1);
    if (!clb) {
        print("allocate_va failed\n");
        free_folio(fb_pa);
        free_folio(clb_pa);
        return;
    }

    fb = allocate_va(1);
    if (!fb) {
        print("allocate_va failed\n");
        free_va(clb, 1);
        free_folio(fb_pa);
        free_folio(clb_pa);
        return;
    }

    if (!map_page(clb_pa, clb, mapping_type::no_cache)) {
        print("map_page failed\n");
        free_va(fb, 1);
        free_va(clb, 1);
        free_folio(fb_pa);
        free_folio(clb_pa);
        return;
    }

    if (!map_page(fb_pa, fb, mapping_type::no_cache)) {
        print("map_page failed\n");
        unmap_page(clb);
        free_va(fb, 1);
        free_va(clb, 1);
        free_folio(fb_pa);
        free_folio(clb_pa);
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
        free_folio(fb_pa);
        free_folio(clb_pa);
        return;
    }

    for (unsigned int i = 0; i < cmdtbl_pages; i++) {
        auto pa = allocate_page();
        if (!pa) {
            print("allocate_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                auto pa = va_to_pa_lookup(va3 + (j << PAGE_SHIFT));

                unmap_page(va3 + (j << PAGE_SHIFT));
                free_folio(pa);
            }

            free_va(va3, cmdtbl_pages);
            unmap_page(fb);
            unmap_page(clb);
            free_va(fb, 1);
            free_va(clb, 1);
            free_folio(fb_pa);
            free_folio(clb_pa);
            return;
        }

        auto cmd_tbls = (HBA_CMD_HEADER*)clb;

        for (unsigned int j = 0; j < PAGE_SIZE / sizeof(HBA_CMD_TBL); j++) {
            auto& tbl = cmd_tbls[(i * PAGE_SIZE / sizeof(HBA_CMD_TBL)) + j];

            tbl.ctba = (uint32_t)(pa + (j * sizeof(HBA_CMD_TBL)));
            tbl.ctbau = (uint32_t)(pa >> 32);
        }

        if (!map_page(pa, va3 + (i << PAGE_SHIFT), mapping_type::no_cache)) {
            print("map_page failed\n");

            free_folio(pa);

            for (unsigned int j = 0; j < i; j++) {
                auto pa = va_to_pa_lookup(va3 + (j << PAGE_SHIFT));

                unmap_page(va3 + (j << PAGE_SHIFT));
                free_folio(pa);
            }

            free_va(va3, cmdtbl_pages);
            unmap_page(fb);
            unmap_page(clb);
            free_va(fb, 1);
            free_va(clb, 1);
            free_folio(fb_pa);
            free_folio(clb_pa);
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

    while (port.cmd.cr) { }

    port.cmd.fre = 1;
    port.cmd.st = 1;

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
    auto& sc = *(sata_card*)ctx;
    auto port_num = (uint8_t)(uintptr_t)ctx2;

    volatile auto& p = sc.abar->ports[port_num];

    if (!sc.reset_port(p)) {
        if (--sc.init_left == 0)
            set_event(static_cast<event&>(sc.init_event.get()));
        return;
    }

    // FIXME - SATA_SIG_ATAPI (CD drives etc.)

    if (p.sig != SATA_SIG_ATA) {
        if (--sc.init_left == 0)
            set_event(static_cast<event&>(sc.init_event.get()));
        return;
    }

    auto pdev = new sata_device(p);
    auto& dev = *pdev;

    if (!dev.valid) {
        delete &dev;

        if (--sc.init_left == 0)
            set_event(static_cast<event&>(sc.init_event.get()));

        return;
    }

    dev.port_num = port_num;

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

    dev.sector_size = 512; // FIXME - get from identify block
    dev.size = (uint64_t)dev.identify_block.user_addressable_logical_sectors * (uint64_t)dev.sector_size;
    dev.dev_num = num_sata_devices.fetch_add(1);

    // FIXME - make sure SATA devices get numbered consistently

    if (dev.dev_num < 26) {
        char name[4];

        name[0] = 's';
        name[1] = 'd';
        name[2] = (char)(dev.dev_num + 'a');
        name[3] = 0;

        auto status = name_object(dev, name);
        if (!success(status)) {
            print("name_object returned {}\n", status);
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

    print("found /dev/{}: {}, block size {}, capacity {}\n", string_view(dev.volume_name, dev.volume_name_len),
          dev.model_number, byte_size(dev.sector_size), byte_size(dev.size));

    dev.probe();

    dev.release();

end:
    if (--sc.init_left == 0)
        set_event(static_cast<event&>(sc.init_event.get()));
}

void sata_device::isr() {
    auto is = port.is;

    port.is = is;

    if (!(is & SATA_IS_DHRS) && !(is & SATA_IS_PSS))
        return;

    // D2H FIS received
    auto ci = port.ci;
    auto signalled = pending_slots & ~ci;

    for (unsigned int i = 0; i < SATA_NUM_CMD_TABLES; i++) {
        if (signalled & (1 << i)) {
            set_event(static_cast<event&>(events[i].get()));
            pending_slots &= ~((uint32_t)1 << i);
        }
    }
}

bool sata_card::isr() {
    auto abar_is = abar->is;

    abar->is = abar_is;

    if (abar_is == 0)
        return false;

    lock_guard lock(port_list_lock);

    for (auto le = port_list.next; le != &port_list; le = le->next) {
        auto& dev = containing_record<sata_device>(le);

        if (abar_is & 1 << dev.port_num) {
            dev.isr();

            abar_is &= ~((uint32_t)1 << dev.port_num);
            if (abar_is == 0)
                break;
        }
    }

    return true;
}

static bool sata_isr_func(void* ctx) {
    return ((sata_card*)ctx)->isr();
}

static bool wait_until(unsigned int ms, const invocable auto& func) {
    auto tim_ref = create_timer(true);
    if (!tim_ref) {
        print("wait_until: create_timer failed\n");
        return false;
    }

    auto& tim = static_cast<timer&>(tim_ref.get());

    auto status = tim.set(-ONE_MS_TICKS, nullptr, nullptr, false,
                          1, nullptr);
    if (!success(status)) {
        print("wait_until: timer::set returned {}\n", status);
        return false;
    }

    for (unsigned int i = 0; i < ms; i++) {
        if (func())
            return true;

        wait_for_object(tim, true); // wait 1ms
    }

    return false;
}

bool sata_card::reset_port(volatile HBA_PORT& p) {
    // see section 10.1 of AHCI spec

    p.cmd.st = 0;
    p.cmd.cr = 0;
    p.cmd.fre = 0;
    p.cmd.fr = 0;
    p.sctl.det = 0;

    // FIXME - should we be setting clb, clbu, fb, and fbu here?

    p.cmd.fre = 1;
    p.cmd.sud = 1;

    if (!wait_until(10, [&]() { return p.ssts.det == 1 || p.ssts.det == 3; })) {
        // FIXME - free clb and fb memory?
        return false;
    }

    p.serr = 0xffffffff;

    // wait up to 32s for device to be ready

    if (!wait_until(32000, [&]() { return !p.tfd.sts_bsy && !p.tfd.sts_drq && !p.tfd.sts_err; })) {
        // FIXME - free clb and fb memory?
        return false;
    }

    return true;
}

static bool init_sata(const pci_device& dev) {
    auto abarmem = dev.bar[5];

    // FIXME - also support MSI-X?

    if (abarmem.empty()) {
        print("ABAR not set for SATA device\n");
        return false;
    }

    if (abarmem.size() < offsetof(HBA_MEM, ports)) {
        print("ABAR was shorter than expected\n");
        return false;
    }

    auto sc = new sata_card(dev);
    if (!sc) {
        print("new failed\n");
        return false;
    }

    sc->abar = (HBA_MEM*)abarmem.data();

    // FIXME - BIOS handoff?

    if (!dev.msi.empty() && msi_supported) {
#ifdef __aarch64__
        auto int_num = allocate_msi_irq();
        if (int_num == 0) {
            print("allocate_msi_irq failed\n");
            return false;
        }

        set_irq_edge_triggered(int_num, true);

        if (!add_irq_handler(int_num, sata_isr_func, sc)) {
            free_msi_irq(int_num);
            print("add_irq_handler failed\n");
            return false;
        }

        sc->msi_irq = int_num;
#else
        auto int_num = add_isr(sata_isr_func, sc);
        if (int_num == 0) {
            print("add_isr failed\n");
            return false;
        }
#endif

        sc->isr_registered = true;

        auto& msi = *(volatile pci_msi*)dev.msi.data();

#ifdef __aarch64__
        msi.message_address_low = msi_frame_setspi & 0xffffffff;
#else
        msi.message_address_low = apic_msi_address(0, false, false); // FIXME - spread out amongst processors
#endif

        if (msi.message_control & 0x80) { // 64-bit
#ifdef __aarch64__
            msi.v64.message_address_high = msi_frame_setspi >> 32;
#else
            msi.v64.message_address_high = 0;
#endif
            msi.v64.message_data = int_num;
        } else
            msi.v32.message_data = int_num;

        msi.message_control = msi.message_control | 1; // enable
    } else if (dev.irq.has_value()) {
        if (!add_irq_handler(*dev.irq, sata_isr_func, sc)) {
            print("add_irq_handler failed\n");
            return false;
        }

        sc->isr_registered = true;
    } else {
        print("init_sata: no MSI and no IRQ found\n");
        return false;
    }

    sc->abar->ghc.ae = 1; // enable ACPI

    sc->abar->is = 0xffffffff; // clear pending interrupts
    sc->abar->ghc.ie = 1; // enable interrupts

    auto ports_count = sc->abar->cap.np + 1;

    uint32_t mask = sc->abar->pi;

    {
        lock_guard lock(sata_list_lock);
        insert_tail_list(sata_list, sc->le);
    }

    sc->init_event = create_event(true);
    sc->init_left = 1;

    for (uint8_t i = 0; i < ports_count; i++) {
        if (!(mask & 1)) {
            mask >>= 1;
            continue;
        }

        sc->init_left++;
        add_kernel_thread(init_sata_device, sc, (void*)(uintptr_t)i);

        mask >>= 1;
    }

    if (--sc->init_left == 0)
        set_event(static_cast<event&>(sc->init_event.get()));

    wait_for_object(sc->init_event.get(), false);

    return true;
}

nt_status sata_device::dio_write(uint64_t page_num, uint64_t num_pages,
                                 pfn_iterator& it, uint64_t& written_pages,
                                 bool fua) {
    // FIXME - proper error handling
    // FIXME - asynchronous mode

    {
        shared_lock lock(flush_sem);

        port.is = 0xffffffff; // clear interrupt bits

        uint8_t slot = get_slot();

        auto& cmd = *(HBA_CMD_HEADER*)(clb + (slot * sizeof(HBA_CMD_HEADER)));

        cmd.cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
        cmd.w = 1; // write
        cmd.prdbc = 0;

        auto& cmdtbl = *cmd_tables[slot];

        memset(&cmdtbl, 0, offsetof(HBA_CMD_TBL, prdt_entry) + (cmd.prdtl * sizeof(HBA_PRDT_ENTRY)));

        if (num_pages > NUM_PRDT_ENTRIES)
            num_pages = NUM_PRDT_ENTRIES;

        for (unsigned int i = 0; i < num_pages; i++) {
            auto& e = cmdtbl.prdt_entry[i];

            if (i != 0)
                it++;

            // FIXME - make sure device supports 64-bit PAs

            e.dba = (uint32_t)*it;
            e.dbau = (uint32_t)(*it >> 32);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            e.dbc = (unsigned int)(PAGE_SIZE - 1); // FIXME - what if device not whole number of pages?
#pragma GCC diagnostic pop
            e.i = 0; // interrupt
        }

        cmdtbl.prdt_entry[num_pages - 1].i = 1;

        cmd.prdtl = (uint16_t)num_pages;

        auto& cmdfis = *(FIS_REG_H2D*)&cmdtbl.cfis;

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
        port.ci = 1 << slot;

        wait_for_object(events[slot].get(), false);

        if (port.is & HBA_PxIS_TFES) {
            print("Error writing to disk\n");
            release_slot(slot);
            return nt_status::INTERNAL_ERROR; // FIXME - translate to nt_status properly
        }

        release_slot(slot);
    }

    // do a flush, as WRITE DMA FUA EXT is apparently hardly ever supported
    if (num_pages != 0 && fua)
        flush();

    written_pages = num_pages;

    return nt_status::SUCCESS;
}

sata_card::~sata_card() {
    if (!isr_registered)
        return;

    // FIXME
    if (!dev.msi.empty()) {
#ifdef __aarch64__
        remove_irq_handler(msi_irq, sata_isr_func, this);
        free_msi_irq(msi_irq);
#else
        remove_isr(sata_isr_func, this);
#endif
    } else if (dev.irq.has_value())
        remove_irq_handler(*dev.irq, sata_isr_func, this);
}

nt_status sata_device::flush() {
    lock_guard lock(flush_sem);

    // FIXME - proper error handling
    // FIXME - asynchronous mode

    port.is = 0xffffffff; // clear interrupt bits

    uint8_t slot = get_slot();

    auto& cmd = *(HBA_CMD_HEADER*)(clb + (slot * sizeof(HBA_CMD_HEADER)));

    cmd.cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmd.w = 0; // read
    cmd.prdbc = 0;

    auto& cmdtbl = *cmd_tables[slot];

    memset(&cmdtbl, 0, sizeof(cmdtbl));

    auto& cmdfis = *(FIS_REG_H2D*)&cmdtbl.cfis;

    cmdfis.fis_type = FIS_TYPE::REG_H2D;
    cmdfis.c = 1; // command
    cmdfis.command = ata_command::FLUSH_CACHE_EX;

    cmdfis.device = 0;

    pending_slots |= 1 << slot;
    port.ci = 1 << slot;

    wait_for_object(events[slot].get(), false);

    if (port.is & HBA_PxIS_TFES) {
        print("Error flushing disk\n");
        release_slot(slot);
        // FIXME - translate into nt_status properly
        return nt_status::INTERNAL_ERROR;
    }

    release_slot(slot);

    return nt_status::SUCCESS;
}
