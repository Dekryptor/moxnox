#pragma once

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

struct xhci_port_regs {
    uint32_t PORTSC;
    uint32_t PORTPMSC;
    uint32_t PORTLI;
    uint32_t PORTHLPMC;
};

static_assert(sizeof(xhci_port_regs) == 0x10);

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
    uint8_t reserved3[964];
    xhci_port_regs ports[256];
} __attribute((packed));

static_assert(sizeof(xhci_op_regs) == 0x1400);

struct xhci_int_regs {
    uint32_t IMAN;
    uint32_t IMOD;
    uint32_t ERSTSZ;
    uint32_t reserved;
    uint64_t ERSTBA;
    uint64_t ERDP;
} __attribute((packed));

static_assert(sizeof(xhci_int_regs) == 0x20);

static const size_t MAX_INT_REGS = 1024;

struct xhci_run_regs {
    uint32_t MFINDEX;
    uint8_t reserved[28];
    xhci_int_regs int_regs[MAX_INT_REGS];
} __attribute((packed));

static_assert(sizeof(xhci_run_regs) == 0x8020);

struct xhci_event_ring_segment {
    uint64_t base_address;
    uint16_t size;
    uint16_t reserved1;
    uint32_t reserved2;
} __attribute((packed));

static_assert(sizeof(xhci_event_ring_segment) == 0x10);
