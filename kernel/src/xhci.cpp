module;

#include "kernel.h"

export module xhci;

import pci;
import print;

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
    uint32_t CRCR;
    uint32_t padding1;
    uint32_t reserved2[4];
    uint32_t DCBAAP;
    uint32_t padding2;
    uint32_t CONFIG;
} __attribute((packed));

static_assert(sizeof(xhci_op_regs) == 0x3c);

export
extern "C++"
void init_xhci(const pci_device& dev) {
    print("FIXME - init_xhci\n");

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

    op_regs.USBCMD = 0x2; // reset

    // wait for reset to complete
    do {
        pit_delay(PIT_HZ / 1000); // wait 1ms
    } while (op_regs.USBCMD & 0x2);

    // FIXME
}
