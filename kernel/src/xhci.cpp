module;

#include "kernel.h"

export module xhci;

import pci;
import print;
import mem;
import config;

struct xhci_cap_regs {
    uint8_t CAPLENGTH;
    uint8_t reserved;
    uint16_t HCIVERSION;
    uint32_t HCSPARAMS1;
    uint32_t HCSPARAMS2;
    uint32_t HCSPARAMS3;
    uint32_t HCCPARAMS1;
    uint32_t DBOFF;
    uint32_t RTSOFF;
    uint32_t HCCPARAMS2;
} __attribute((packed));

static_assert(sizeof(xhci_cap_regs) == 0x20);

struct xhci_op_regs {
    uint32_t USBCMD;
    uint32_t USBSTS;
    uint32_t PAGESIZE;
    uint32_t reserved1[2];
    uint32_t DNCTRL;
    uint64_t CRCR;
    uint32_t reserved2[4];
    uint64_t DCBAAP;
    uint32_t CONFIG;
} __attribute((packed));

static_assert(sizeof(xhci_op_regs) == 0x3c);

class xhci {
public:
    xhci(pci_device& dev);
    ~xhci();

    bool initialized = false;

private:
    pci_device& dev;
    uint8_t num_slots;
    uint64_t* dcbaa = nullptr;
    void* command_ring = nullptr;
};

static const uint32_t XHCI_USBCMD_RS = 0x00000001;
static const uint32_t XHCI_USBCMD_HCRST = 0x00000002;

static void write_volatile64(volatile uint64_t* addr, uint64_t val) {
    *(volatile uint32_t*)addr = val & 0xffffffff;
    *((volatile uint32_t*)addr + 1) = (uint32_t)(val >> 32);
}

xhci::xhci(pci_device& dev) : dev(dev) {
    if (dev.bar[0].size() < sizeof(xhci_cap_regs)) {
        print("init_xhci: BAR0 was {} bytes, expected at least {}\n",
              dev.bar[0].size(), sizeof(xhci_cap_regs));
        return;
    }

    volatile auto& cap_regs = *(xhci_cap_regs*)dev.bar[0].data();

    if (dev.bar[0].size() < cap_regs.CAPLENGTH + sizeof(xhci_op_regs)) {
        print("init_xhci: BAR0 was {} bytes, expected at least {}\n",
              dev.bar[0].size(), cap_regs.CAPLENGTH + sizeof(xhci_op_regs));
        return;
    }

    volatile auto& op_regs = *(xhci_op_regs*)((uint8_t*)&cap_regs + cap_regs.CAPLENGTH);

    op_regs.USBCMD &= ~XHCI_USBCMD_RS; // unset run bit
    for (unsigned int i = 0; i < 16; i++) { // specs say we should wait up to 16ms
        if (op_regs.USBSTS & 1) // halted
            break;

        pit_delay(PIT_HZ / 1000); // wait 1ms
    }

    op_regs.USBCMD = XHCI_USBCMD_HCRST; // reset

    // wait for reset to complete
    do {
        pit_delay(PIT_HZ / 1000); // wait 1ms
    } while (op_regs.USBCMD & XHCI_USBCMD_HCRST);

    auto hcsparams1 = cap_regs.HCSPARAMS1;

    num_slots = hcsparams1 & 0xff;

    print("init_xhci: num_slots = {}\n", num_slots);

    dcbaa = (uint64_t*)malloc(num_slots * sizeof(uint64_t));
    if (!dcbaa) {
        print("malloc failed\n");
        return;
    }

    memset(dcbaa, 0, num_slots * sizeof(uint64_t));

    write_volatile64(&op_regs.DCBAAP, va_to_pa(dcbaa));

    // FIXME - calculate how big command ring should be (PAGE_SIZE for now)
    auto pa = allocate_folio(0);
    if (!pa) {
        print("allocate_folio failed\n");
        return;
    }

    command_ring = phys_address(pa);
    memset(command_ring, 0, PAGE_SIZE);

    write_volatile64(&op_regs.CRCR, pa | 1);

    op_regs.CONFIG = (op_regs.CONFIG & ~(uint32_t)0xff) | num_slots;

    op_regs.USBCMD |= XHCI_USBCMD_RS;

    initialized = true;
}

xhci::~xhci() {
    if (dcbaa)
        free(dcbaa);

    if (command_ring)
        free_folio(phys_memory_pfn(command_ring) << PAGE_SHIFT);
}

export
extern "C++"
void init_xhci(pci_device& dev) {
    print("FIXME - init_xhci\n");

    xhci x(dev);

    if (!x.initialized)
        return;

    // FIXME
}
