module;

#include <stdint.h>
#include "tick_rate.h"

import pci;
import module_init;
import list;
import print;
import interrupts;
import thread;
import scheduler;
import object_ref;
import event;
import timer;
import config;
import nt;

export module sdhci;

using namespace std;

#define ONE_MS_TICKS (SYSTEM_TIME_RESOLUTION / 1000) // FIXME

union sdhci_clkcon {
    struct {
        uint16_t internal_clock_enable : 1;
        uint16_t internal_clock_stable : 1;
        uint16_t sd_clock_enable : 1;
        uint16_t pll_enable : 1;
        uint16_t : 1;
        uint16_t clock_generator_select : 1;
        uint16_t sdclk_frequency_select_upper : 2;
        uint16_t sdclk_frequency_select : 8;
    };
    uint16_t value;
};

static_assert(sizeof(sdhci_clkcon) == 0x2);

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

union sdhci_cmdreg {
    struct {
        uint16_t response_type_select : 2;
        uint16_t : 1;
        uint16_t command_crc_check_enable : 1;
        uint16_t command_index_check_enable : 1;
        uint16_t data_present_select : 1;
        uint16_t command_type : 2;
        uint16_t command_index : 6;
        uint16_t : 2;
    };
    uint16_t value;
};

static_assert(sizeof(sdhci_cmdreg) == 0x2);

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
    sdhci_clkcon clkcon;
    uint8_t timeoutcon;
    uint8_t swrst;
    uint16_t norintsts;
    uint16_t errintsts;
    sdhci_norint norintstsen;
    uint16_t errintstsen;
    sdhci_norint norintsigen;
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

enum class sd_cmd : uint16_t {
    GO_IDLE_STATE = 0,
    ALL_SEND_CID = 2,
    SEND_RELATIVE_ADDR = 3,
    SEND_DSR = 4,
    SWITCH_FUNC = 6,
    SELECT_CARD = 7,
    SEND_IF_COND = 8,
    SEND_CSD = 9,
    SEND_CID = 10,
    VOLTAGE_SWITCH = 11,
    STOP_TRANSMISSION = 12,
    SEND_STATUS = 13,
    GO_INACTIVE_STATE = 15,
    SET_BLOCKLEN = 16,
    READ_SINGLE_BLOCK = 17,
    READ_MULTIPLE_BLOCK = 18,
    SEND_TUNING_BLOCK = 19,
    SPEED_CLASS_CONTROL = 20,
    ADDRESS_EXTENSION = 22,
    SET_BLOCK_COUNT = 23,
    WRITE_SINGLE_BLOCK = 24,
    WRITE_MULTIPLE_BLOCK = 25,
    PROGRAM_CSD = 27,
    SET_WRITE_PROT = 28,
    CLR_WRITE_PROT = 29,
    SEND_WRITE_PROT = 30,
    ERASE_WR_BLK_START = 32,
    ERASE_WR_BLK_END = 33,
    ERASE = 38,
    SELECT_CARD_PARTITION = 39,
    LOCK_UNLOCK = 42,
    Q_MANAGEMENT = 43,
    Q_TASK_INFO_A = 44,
    Q_TASK_INFO_B = 45,
    Q_RD_TASK = 46,
    Q_WR_TASK = 47,
    READ_EXTR_SINGLE = 48,
    WRITE_EXTR_SINGLE = 49,
    APP_CMD = 55,
    GEN_CMD = 56,
    READ_EXTR_MULTI = 58,
    WRITE_EXTR_MULTI = 59,
};

enum class response_type {
    none,
    R1,
    R1b,
    R2,
    R3,
    R4,
    R5,
    R5b,
    R6,
    R7,
};

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
    void thread();
    void card_removal();
    void card_insertion();
    bool set_clock(unsigned int freq);
    void send_command(enum sd_cmd cmd, uint32_t arg);

    list_entry le;
    sdhci_device& d;
    volatile sdhci_regs& regs;
    object_ref thread_event, command_complete_event;
    bool card_removal_pending = false;
    bool card_insertion_pending = false;
    unsigned int base_clock_frequency;
    bool initialized = false;
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
        card_removal_pending = true;
        set_event(static_cast<event&>(thread_event.get()));

        norintsts.value = 0;
        norintsts.card_removal = 1;
        regs.norintsts = norintsts.value;

        return true;
    }

    if (norintsts.card_insertion) {
        card_insertion_pending = true;
        set_event(static_cast<event&>(thread_event.get()));

        norintsts.value = 0;
        norintsts.card_insertion = 1;
        regs.norintsts = norintsts.value;

        return true;
    }

    if (norintsts.command_complete) {
        set_event(static_cast<event&>(command_complete_event.get()));

        norintsts.value = 0;
        norintsts.command_complete = 1;
        regs.norintsts = norintsts.value;

        return true;
    }

    return false;
}

void sdhci_slot::card_removal() {
    print("FIXME - sdhci_slot::card_removal\n");

    // FIXME
}

bool sdhci_slot::set_clock(unsigned int freq) {
    unsigned int div;

    // stop clock

    while (true) {
        sdhci_prnsts prnsts;

        prnsts.value = regs.prnsts;

        if (!prnsts.command_inhibit_cmd && !prnsts.command_inhibit_dat)
            break;

        // FIXME - wait 1ms?
        // FIXME - timeout
    }

    regs.clkcon.value = 0;

    // version 2 divisors must be power of 2
    // FIXME - easier from v3 onward (see Linux's sdhci_calc_clk)

    for (div = 1; div < 256; div <<= 1) {
        if ((base_clock_frequency / div) <= freq)
            break;
    }

    sdhci_clkcon clkcon;

    clkcon.value = 0;

    clkcon.sdclk_frequency_select = (uint8_t)(div >> 1);
    clkcon.internal_clock_enable = 1;

    regs.clkcon.value = clkcon.value;

    // wait 150ms for regs.clkcon.internal_clock_stable to be 1

    {
        auto tim_ref = create_timer(true);
        if (!tim_ref) {
            print("sdhci_slot::set_clock: create_timer failed\n");
            return false;
        }

        auto& tim = static_cast<timer&>(tim_ref.get());

        auto status = tim.set(-ONE_MS_TICKS, nullptr, nullptr, false, 1, nullptr);
        if (!success(status)) {
            print("sdhci_slot::set_clock: timer::set returned {}\n", status);
            return false;
        }

        bool done = false;
        for (unsigned int i = 0; i < 150; i++) {
            if (regs.clkcon.internal_clock_stable) {
                done = true;
                break;
            }

            wait_for_object(tim, true); // wait 1ms
        }

        if (!done) {
            print("sdhci_slot::set_clock: timeout\n");
            return false;
        }
    }

    regs.clkcon.sd_clock_enable = 1;

    return true;
}

void sdhci_slot::send_command(enum sd_cmd cmd, uint32_t arg) {
    // FIXME - do we need a mutex?

    reset_event(static_cast<event&>(command_complete_event.get()));
    regs.norintstsen.command_complete = 1;

    response_type type;

    switch (cmd) {
        using enum sd_cmd;

        case GO_IDLE_STATE:
            type = response_type::none; // FIXME? docs say R1
            break;

        default:
            print("sdhci_slot::send_command: unhandled command type {}\n", (uint16_t)cmd);
            return;
    }

    sdhci_cmdreg cmdreg;

    cmdreg.value = 0;

    cmdreg.command_index = (uint16_t)cmd & 0x3f;

    switch (type) {
        using enum response_type;

        case none:
            cmdreg.response_type_select = 0;
            cmdreg.command_index_check_enable = 0;
            cmdreg.command_crc_check_enable = 0;
        break;

        case R2:
            cmdreg.response_type_select = 1;
            cmdreg.command_index_check_enable = 0;
            cmdreg.command_crc_check_enable = 1;
        break;

        case R3:
        case R4:
            cmdreg.response_type_select = 2;
            cmdreg.command_index_check_enable = 0;
            cmdreg.command_crc_check_enable = 0;
        break;

        case R1:
        case R5:
        case R6:
        case R7:
            cmdreg.response_type_select = 2;
            cmdreg.command_index_check_enable = 1;
            cmdreg.command_crc_check_enable = 1;
        break;

        case R1b:
        case R5b:
            cmdreg.response_type_select = 3;
            cmdreg.command_index_check_enable = 1;
            cmdreg.command_crc_check_enable = 1;
        break;
    }

    regs.argument = arg;
    regs.cmdreg = cmdreg.value;

    wait_for_object(command_complete_event.get(), false); // FIXME - timeout (still clear norintstsen flag)

    regs.norintstsen.command_complete = 0;
}

void sdhci_slot::card_insertion() {
    print("FIXME - sdhci_slot::card_insertion\n");

    // FIXME - check card is actually inserted
    // FIXME - what do we do if we think there's a card already there?

    if (!set_clock(400000)) {
        print("sdhci_slot::card_insertion: set_clock failed\n");
        return;
    }

    // FIXME - set timeout control

    send_command(sd_cmd::GO_IDLE_STATE, 0);
}

void sdhci_slot::thread() {
    // FIXME - stopping thread?

    // FIXME - racy?

    while (true) {
        wait_for_object(thread_event.get(), false);

        if (card_removal_pending) {
            card_removal();
            card_removal_pending = false;
        }

        if (card_insertion_pending) {
            card_insertion();
            card_insertion_pending = false;
        }
    }
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

    regs.norintstsen.value = norintstsen.value;

    norintsigen.value = 0;

    norintsigen.card_insertion = 1;
    norintsigen.card_removal = 1;
    norintsigen.command_complete = 1;

    regs.norintsigen.value = norintsigen.value;

    // get base clock frequency

    sdhci_capab capab;

    capab.low = regs.capab[0];
    capab.high = regs.capab[1];

    if (capab.base_clock_frequency == 0) {
        print("sdhci_slot::sdhci_slot: base clock frequency was 0\n");
        return;
    }

    base_clock_frequency = (unsigned int)capab.base_clock_frequency * 1000000;

    // start thread

    thread_event = create_event(true);
    command_complete_event = create_event(false);

    sdhci_prnsts prnsts;

    prnsts.value = regs.prnsts;

    if (prnsts.card_inserted) {
        card_insertion_pending = true;
        set_event(static_cast<event&>(thread_event.get()));
    }

    add_kernel_thread([](void* ctx) {
        auto& s = *(sdhci_slot*)ctx;

        s.thread();
    }, this);

    initialized = true;
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

    if (!s->initialized) {
        print("sdhci_slot::sdhci_slot failed\n");
        delete s;
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
        delete d;
        return false;
    }

    // FIXME - put in list somewhere?

    return true;
}
