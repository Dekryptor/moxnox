module;

#include <stdint.h>

import pci;
import module_init;
import list;
import print;

export module sdhci;

using namespace std;

struct sdhci_regs {
    uint32_t sysad;
    uint16_t blksize;
    uint16_t blkcnt;
    uint32_t argument;
    uint16_t trnmod;
    uint16_t cmdreg;
    uint32_t rspreg0;
    uint32_t rspreg1;
    uint32_t rspreg2;
    uint32_t rspreg3;
    uint32_t bdata;
    uint32_t prnsts;
    uint8_t hostctl;
    uint8_t pwrcon;
    uint8_t blkgap;
    uint8_t wakcon;
    uint16_t clkcon;
    uint8_t timeoutcon;
    uint8_t swrst;
    uint16_t norintsts;
    uint16_t errintsts;
    uint16_t norintstsen;
    uint16_t errintstsen;
    uint16_t norintsigen;
    uint16_t errintsigen;
    uint16_t acmd12errsts;
    uint16_t hostctl2;
    uint32_t capab[2];
    uint32_t maxcurr[2];
    uint16_t feaer;
    uint16_t feerr;
    uint32_t admaerr;
    uint32_t admasysaddr[2];
    uint8_t other_regs[156];
    uint16_t slot_int_status;
    uint16_t hcver;
} __attribute((packed));

static_assert(sizeof(sdhci_regs) == 0x100);

static bool init_sdhci(const pci_device& dev);

static pci_class_recognizer mod_pci_class = {
    {}, pci_class::base_system_peripheral, (uint8_t)pci_base_system_peripheral::sdhci, true, init_sdhci
};

static module_init module_init_xhci([]() {
    insert_tail_list(pci_classes, mod_pci_class.le);
});

static const char* spec_version_to_string(uint8_t ver) {
    switch (ver) {
        case 0:
            return "1.00";
        case 1:
            return "2.00";
        case 2:
            return "3.00";
        case 3:
            return "4.00";
        case 4:
            return "4.10";
        case 5:
            return "4.20";
        default:
            return "?";
    }
}

static bool init_sdhci(const pci_device& dev) {
    print("FIXME - init_sdhci\n"); // FIXME

    if (dev.bar[0].size() < sizeof(sdhci_regs)) {
        print("init_sdhci: BAR 0 was {} bytes, expected {}\n",
              dev.bar[0].size(), sizeof(sdhci_regs));
        return false;
    }

    volatile auto& regs = *(sdhci_regs*)dev.bar[0].data();

    auto hcver = *(volatile uint16_t*)&regs.hcver;

    print("SDHCI version {}\n", spec_version_to_string((uint8_t)(hcver & 0xff)));

    return false;
}
