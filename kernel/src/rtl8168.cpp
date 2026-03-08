#include <stdint.h>

import print;
import pci;
import list;
import module_init;
import net;

static const uint16_t PCI_VENDOR_ID_REALTEK = 0x10ec;

static const size_t RTL_IDR0 = 0x00;
static const size_t RTL_MAR0 = 0x08;
static const size_t RTL_TxDescStartAddrLow = 0x20;
static const size_t RTL_TxDescStartAddrHigh = 0x24;
static const size_t RTL_CR = 0x37;
static const size_t RTL_TPPOLL = 0x38;
static const size_t RTL_IMR = 0x3c;
static const size_t RTL_ISR = 0x3e;
static const size_t RTL_TCR = 0x40;
static const size_t RTL_RCR = 0x44;
static const size_t RTL_CONFIG0 = 0x51;
static const size_t RTL_CONFIG1 = 0x52;
static const size_t RTL_CONFIG2 = 0x53;
static const size_t RTL_CONFIG3 = 0x54;
static const size_t RTL_CONFIG4 = 0x55;
static const size_t RTL_CONFIG5 = 0x56;
static const size_t RTL_PHYAR = 0x60;
static const size_t RTL_RxDescAddrLow = 0xe4;
static const size_t RTL_RxDescAddrHigh = 0xe8;

void found_rtl8168(pci_device& dev);

static pci_recognizer mod_pci_ids[] = {
    { {}, PCI_VENDOR_ID_REALTEK, 0x8168, true, found_rtl8168 },
};

static void init_rtl8168() {
    for (unsigned int i = 0; i < sizeof(mod_pci_ids) / sizeof(*mod_pci_ids); i++) {
        insert_tail_list(pci_ids, mod_pci_ids[i].le);
    }
}

static module_init module_init_rtl8168(init_rtl8168);

void found_rtl8168(pci_device& dev) {
    print("FIXME - found_rtl8168\n"); // FIXME

    auto mmio = dev.bar[2];

    if (mmio.empty()) {
        print("found_rtl8168: BAR2 not set\n");
        return;
    }

    mac_address mac;

    for (unsigned int i = 0; i < 6; i++) {
        mac.v[i] = mmio[RTL_IDR0 + i];
    }

    print("MAC address: {}\n", mac);

    uint32_t tcr = *(volatile uint32_t*)(mmio.data() + RTL_TCR);

    print("TCR = {:08x}\n", tcr);
    print("MAC version = {:08x}\n", tcr & 0x7cf00000);
}
