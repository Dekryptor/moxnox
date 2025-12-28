module;

#include <stdint.h>

import pci;
import module_init;
import list;
import print;
import interrupts;

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

union sdhci_norint {
    struct {
        uint16_t command_complete : 1;
        uint16_t transfer_complete : 1;
        uint16_t block_gap_event : 1;
        uint16_t dma_interrupt : 1;
        uint16_t buffer_write_ready : 1;
        uint16_t buffer_read_ready : 1;
        uint16_t card_insertion : 1;
        uint16_t card_removal : 1;
        uint16_t card_interrupt : 1;
        uint16_t int_a : 1;
        uint16_t int_b : 1;
        uint16_t int_c : 1;
        uint16_t retuning_event : 1;
        uint16_t fx_event : 1;
        uint16_t : 2;
    };
    uint16_t value;
};

static_assert(sizeof(sdhci_norint) == 0x2);

union sdhci_prnsts {
    struct {
        uint32_t command_inhibit_cmd : 1;
        uint32_t command_inhibit_dat : 1;
        uint32_t dat_line_active : 1;
        uint32_t retuning_request : 1;
        uint32_t dat_line_signal_level : 4;
        uint32_t write_transfer_active : 1;
        uint32_t read_transfer_active : 1;
        uint32_t buffer_write_enable : 1;
        uint32_t buffer_read_enable : 1;
        uint32_t : 4;
        uint32_t card_inserted : 1;
        uint32_t card_state_stable : 1;
        uint32_t card_detect_pin_level : 1;
        uint32_t write_protect_switch_pin_level : 1;
        uint32_t dat_line_signal_level2 : 4;
        uint32_t cmd_line_signal_level : 1;
        uint32_t host_regulator_voltage_stable : 1;
        uint32_t : 1;
        uint32_t command_not_issued_by_error : 1;
        uint32_t sub_command_status : 1;
        uint32_t in_dormant_state : 1;
        uint32_t lane_synchronization : 1;
        uint32_t uhsii_if_detection : 1;
    };
    uint32_t value;
};

static_assert(sizeof(sdhci_prnsts) == 0x4);

class sdhci_device {
public:
    sdhci_device(const pci_device& dev);
    bool isr();

    const pci_device& dev;
    bool initialized = false;
    list_entry slots = { &slots, &slots };
};

class sdhci_slot {
public:
    sdhci_slot(sdhci_device& d, volatile sdhci_regs& regs);
    bool isr();

    sdhci_device& d;
    volatile sdhci_regs& regs;
    list_entry le;
};

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

bool sdhci_slot::isr() {
    sdhci_norint norintsts;

    norintsts.value = regs.norintsts;

    if (norintsts.card_removal) {
        print("sdhci_slot::isr: card removal\n"); // FIXME

        norintsts.value = 0;
        norintsts.card_removal = 1;
        regs.norintsts = norintsts.value;

        return true;
    }

    if (norintsts.card_insertion) {
        print("sdhci_slot::isr: card insertion\n"); // FIXME

        norintsts.value = 0;
        norintsts.card_insertion = 1;
        regs.norintsts = norintsts.value;

        return true;
    }

    return false;
}

sdhci_slot::sdhci_slot(sdhci_device& d, volatile sdhci_regs& regs) : d(d), regs(regs) {
    auto hcver = *(volatile uint16_t*)&regs.hcver;

    print("SDHCI version {}\n", spec_version_to_string((uint8_t)(hcver & 0xff)));

    // reset

    regs.swrst = 1;

    while (regs.swrst & 1) { } // FIXME - timeout

    // enable interrupts

    sdhci_norint norintstsen, norintsigen;

    norintstsen.value = 0;

    norintstsen.card_insertion = 1;
    norintstsen.card_removal = 1;

    regs.norintstsen = norintstsen.value;

    norintsigen.value = 0;

    norintsigen.card_insertion = 1;
    norintsigen.card_removal = 1;

    regs.norintsigen = norintsigen.value;

    // check present state

    sdhci_prnsts prnsts;

    prnsts.value = regs.prnsts;

    if (prnsts.card_inserted)
        print("FIXME - card insertion\n"); // FIXME
}

static void init_sdhci_slot(sdhci_device& d, uint8_t bar_num) {
    print("FIXME - init_sdhci_slot\n"); // FIXME

    if (d.dev.bar[bar_num].size() < sizeof(sdhci_regs)) {
        print("init_sdhci_slot: BAR {} was {} bytes, expected {}\n",
              bar_num, d.dev.bar[bar_num].size(), sizeof(sdhci_regs));
        return;
    }

    volatile auto& regs = *(sdhci_regs*)d.dev.bar[bar_num].data();

    auto s = new sdhci_slot(d, regs);
    if (!s) {
        print("new failed\n");
        return;
    }

    insert_tail_list(d.slots, s->le);
}

static bool sdhci_isr_func(void* ctx) {
    auto& dev = *(sdhci_device*)ctx;

    return dev.isr();
}

sdhci_device::sdhci_device(const pci_device& dev) : dev(dev) {
    sdhci_slot_info_reg slot_info_reg;

    if (!dev.irq.has_value()) {
        print("sdhci_device::sdhci_device: IRQ for PCI device not found\n");
        return;
    }

    slot_info_reg.value = *(volatile uint8_t*)(dev.header + 0x40);

    for (unsigned int i = 0; i < (unsigned int)(slot_info_reg.num_slots + 1); i++) {
        if (slot_info_reg.first_bar_num + i >= dev.bar.size()) {
            print("sdhci_device::sdhci_device: slot {} would use invalid BAR {}\n",
                  i, slot_info_reg.first_bar_num + i);
            break;
        }

        init_sdhci_slot(*this, (uint8_t)(slot_info_reg.first_bar_num + i));
    }

    if (!add_irq_handler(*dev.irq, true, sdhci_isr_func, this)) {
        print("sdhci_device::sdhci_device: add_irq_handler failed\n");
        return;
    }

    initialized = true;
}

bool sdhci_device::isr() {
    for (auto le = slots.next; le != &slots; le = le->next) {
        auto& s = containing_record<sdhci_slot>(le);

        if (s.isr())
            return true;
    }

    return false;
}

static bool init_sdhci(const pci_device& dev) {
    auto d = new sdhci_device(dev);
    if (!d) {
        print("new failed\n");
        return false;
    }

    if (!d->initialized) {
        print("sdhci_device::sdhci_device failed\n");
        return false;
    }

    // FIXME - put in list somewhere?

    return true;
}
