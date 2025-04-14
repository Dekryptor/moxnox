module;

#include "kernel.h"

export module xhci;

import pci;
import print;
import mem;
import config;

using namespace std;

static uint16_t EVENT_RING_SIZE = 64;

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

class xhci {
public:
    xhci(pci_device& dev);
    ~xhci();
    void init_ports();

    bool initialized = false;

private:
    pci_device& dev;
    uint8_t num_slots, num_ports;
    uint64_t* dcbaa = nullptr;
    void* command_ring = nullptr;
    void* event_ring = nullptr;
    xhci_event_ring_segment* seg_table = nullptr;
};

static const uint32_t XHCI_USBCMD_RS = 0x00000001;
static const uint32_t XHCI_USBCMD_HCRST = 0x00000002;

static const uint32_t XHCI_PORTSC_CCS = 0x00000001;
static const uint32_t XHCI_PORTSC_PED = 0x00000002;
static const uint32_t XHCI_PORTSC_PR = 0x00000010;
static const uint32_t XHCI_PORTSC_PP = 0x00000200;
static const uint32_t XHCI_PORTSC_PIC = 0x0000c000;

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

    if (cap_regs.CAPLENGTH < 0x20) {
        print("init_xhci: CAPLENGTH was {}, expected at least {}\n",
              cap_regs.CAPLENGTH, 0x20);
        return;
    }

    auto hcsparams1 = cap_regs.HCSPARAMS1;

    num_slots = hcsparams1 & 0xff;
    num_ports = (uint8_t)(hcsparams1 >> 24);

    if (dev.bar[0].size() < cap_regs.CAPLENGTH + offsetof(xhci_op_regs, ports) + (num_ports * sizeof(xhci_port_regs))) {
        print("init_xhci: BAR0 was {} bytes, expected at least {}\n",
              dev.bar[0].size(), cap_regs.CAPLENGTH + sizeof(xhci_op_regs) + (num_ports * sizeof(xhci_port_regs)));
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

    bool reset_done = false;

    // wait for reset to complete
    for (unsigned int i = 0; i < 500; i++) {
        if (!(op_regs.USBCMD & XHCI_USBCMD_HCRST)) {
            reset_done = true;
            break;
        }

        pit_delay(PIT_HZ / 1000); // wait 1ms
    }

    if (!reset_done) {
        print("init_xhci: timed out resetting controller\n");
        return;
    }

    print("init_xhci: num_slots = {}, num_ports = {}\n", num_slots, num_ports);

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

    if (dev.bar[0].size() < cap_regs.RTSOFF + offsetof(xhci_run_regs, int_regs) + sizeof(xhci_int_regs)) {
        print("init_xhci: BAR0 was {} bytes, expected at least {}\n",
              dev.bar[0].size(), cap_regs.RTSOFF + offsetof(xhci_run_regs, int_regs) + sizeof(xhci_int_regs));
        return;
    }

    volatile auto& run_regs = *(xhci_run_regs*)((uint8_t*)&cap_regs + cap_regs.RTSOFF);

    // FIXME - calculate how big event ring should be (PAGE_SIZE for now)
    pa = allocate_folio(0);
    if (!pa) {
        print("allocate_folio failed\n");
        return;
    }

    event_ring = phys_address(pa);
    memset(event_ring, 0, PAGE_SIZE);

    seg_table = (xhci_event_ring_segment*)malloc_aligned(sizeof(xhci_event_ring_segment), 64);
    if (!seg_table) {
        print("malloc failed\n");
        return;
    }

    memset(seg_table, 0, sizeof(xhci_event_ring_segment));

    seg_table->base_address = pa;
    seg_table->size = EVENT_RING_SIZE;

    volatile auto& int_reg = run_regs.int_regs[0];

    int_reg.ERSTSZ = 1;
    write_volatile64(&int_reg.ERDP, va_to_pa(event_ring));
    write_volatile64(&int_reg.ERSTBA, va_to_pa(seg_table));

    op_regs.USBCMD |= XHCI_USBCMD_RS;

    initialized = true;
}

xhci::~xhci() {
    if (dcbaa)
        free(dcbaa);

    if (command_ring)
        free_folio(phys_memory_pfn(command_ring) << PAGE_SHIFT);

    if (event_ring)
        free_folio(phys_memory_pfn(event_ring) << PAGE_SHIFT);

    if (seg_table)
        free(seg_table);
}

void xhci::init_ports() {
    volatile auto& cap_regs = *(xhci_cap_regs*)dev.bar[0].data();
    volatile auto& op_regs = *(xhci_op_regs*)((uint8_t*)&cap_regs + cap_regs.CAPLENGTH);

    for (unsigned int i = 0; i < num_ports; i++) {
        auto portsc = op_regs.ports[i].PORTSC;

        print("port {}: status {:08x}\n", i, portsc);

        if (!(portsc & XHCI_PORTSC_CCS)) // ignore if no device connected
            continue;

        // reset port
        portsc &= XHCI_PORTSC_PP | XHCI_PORTSC_PIC;
        portsc |= XHCI_PORTSC_PR;

        op_regs.ports[i].PORTSC = portsc;

        bool port_enabled = false;

        for (unsigned int j = 0; j < 500; j++) {
            if (op_regs.ports[i].PORTSC & XHCI_PORTSC_PED) {
                port_enabled = true;
                break;
            }

            pit_delay(PIT_HZ / 1000); // wait 1ms
        }

        if (!port_enabled) {
            print("timed out waiting for port to be enabled\n");
            continue;
        }

        // FIXME
    }
}

export
extern "C++"
void init_xhci(pci_device& dev) {
    print("FIXME - init_xhci\n");

    xhci x(dev);

    if (!x.initialized)
        return;

    x.init_ports();

    // FIXME
}
