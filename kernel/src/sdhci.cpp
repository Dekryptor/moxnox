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

union sdhci_capab {
    struct {
        uint32_t timeout_clock_frequency : 6;
        uint32_t : 1;
        uint32_t timeout_clock_unit : 1;
        uint32_t base_clock_frequency : 8;
        uint32_t max_block_length : 2;
        uint32_t eight_bit_support : 1;
        uint32_t adma2_support : 1;
        uint32_t : 1;
        uint32_t high_speed_support : 1;
        uint32_t sdma_support : 1;
        uint32_t suspend_support : 1;
        uint32_t v33_support : 1;
        uint32_t v30_support : 1;
        uint32_t v18_support : 1;
        uint32_t v4_64bit_system_address_support : 1;
        uint32_t v3_64bit_system_address_support : 1;
        uint32_t async_interrupt_support : 1;
        uint32_t slot_type : 2;
        uint32_t sdr50_support : 1;
        uint32_t sdr104_support : 1;
        uint32_t ddr50_support : 1;
        uint32_t uhsii_support : 1;
        uint32_t driver_type_a_support : 1;
        uint32_t driver_type_c_support : 1;
        uint32_t driver_type_d_support : 1;
        uint32_t : 1;
        uint32_t timer_count_for_retuning : 4;
        uint32_t : 1;
        uint32_t use_tuning_for_sdr50 : 1;
        uint32_t retuning_modes : 2;
        uint32_t clock_multiplier : 8;
        uint32_t : 3;
        uint32_t adma3_support : 1;
        uint32_t v18_vdd2_support : 1;
        uint32_t : 3;
    };
    struct {
        uint32_t low;
        uint32_t high;
    };
};

static_assert(sizeof(sdhci_capab) == 0x8);

union sdhci_slot_info_reg {
    struct {
        uint8_t first_bar_num : 3;
        uint8_t : 1;
        uint8_t num_slots : 3;
        uint8_t : 1;
    };
    uint8_t value;
};

static_assert(sizeof(sdhci_slot_info_reg) == 0x1);

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

static void init_sdhci_slot(const pci_device& dev, uint8_t bar_num) {
    print("FIXME - init_sdhci_slot\n"); // FIXME

    if (dev.bar[bar_num].size() < sizeof(sdhci_regs)) {
        print("init_sdhci_slot: BAR {} was {} bytes, expected {}\n",
              bar_num, dev.bar[bar_num].size(), sizeof(sdhci_regs));
        return;
    }

    volatile auto& regs = *(sdhci_regs*)dev.bar[bar_num].data();

    auto hcver = *(volatile uint16_t*)&regs.hcver;

    print("SDHCI version {}\n", spec_version_to_string((uint8_t)(hcver & 0xff)));

    // FIXME
}

static bool init_sdhci(const pci_device& dev) {
    sdhci_slot_info_reg slot_info_reg;

    slot_info_reg.value = *(volatile uint8_t*)(dev.header + 0x40);

    for (unsigned int i = 0; i < (unsigned int)(slot_info_reg.num_slots + 1); i++) {
        if (slot_info_reg.first_bar_num + i >= dev.bar.size()) {
            print("init_sdhci: slot {} would use invalid BAR {}\n",
                  i, slot_info_reg.first_bar_num + i);
            break;
        }

        init_sdhci_slot(dev, (uint8_t)(slot_info_reg.first_bar_num + i));
    }

    return true;
}
