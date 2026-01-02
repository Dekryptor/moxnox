module;

#include <stdint.h>
#include <optional>
#include <span>
#include <string_view>
#include <mutex>
#include <atomic>
#include "tick_rate.h"
#include "acpica.h"

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
import format;
import acpi;
import mem;
import rwsem;
import volume;
import page_cache;
import handle;

export module sdhci;

using namespace std;

#define ONE_MS_TICKS (SYSTEM_TIME_RESOLUTION / 1000) // FIXME

static const size_t BLOCK_SIZE = 512;

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
        uint16_t : 1;
        uint16_t error_interrupt : 1;
    };
    uint16_t value;
};

static_assert(sizeof(sdhci_norint) == 0x2);

union sdhci_errint {
    struct {
        uint16_t command_timeout_error : 1;
        uint16_t command_crc_error : 1;
        uint16_t command_end_bit_error : 1;
        uint16_t command_index_error : 1;
        uint16_t data_timeout_error : 1;
        uint16_t data_crc_error : 1;
        uint16_t data_end_bit_error : 1;
        uint16_t current_limit_error : 1;
        uint16_t auto_cmd12_error : 1;
        uint16_t adma_error : 1;
        uint16_t : 2;
        uint16_t vendor_specific_error : 4;
    };
    uint16_t value;
};

static_assert(sizeof(sdhci_errint) == 0x2);

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
    sdhci_norint norintsts;
    sdhci_errint errintsts;
    sdhci_norint norintstsen;
    sdhci_errint errintstsen;
    sdhci_norint norintsigen;
    sdhci_errint errintsigen;
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

union sdhci_pwrcon {
    struct {
        uint8_t vdd1_power : 1;
        uint8_t vdd1_voltage : 3;
        uint8_t vdd2_power : 1;
        uint8_t vdd2_voltage : 3;
    };
    uint8_t value;
};

static_assert(sizeof(sdhci_pwrcon) == 0x1);

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

union sdhci_hostctl {
    struct {
        uint8_t led_control : 1;
        uint8_t data_transfer_width : 1;
        uint8_t high_speed_enable : 1;
        uint8_t dma_select : 2;
        uint8_t : 1;
        uint8_t card_detect_test_level : 1;
        uint8_t card_detect_signal_selection : 1;
    };
    uint8_t value;
};

static_assert(sizeof(sdhci_hostctl) == 0x1);

union sdhci_trnmod {
    struct {
        uint16_t dma_enable : 1;
        uint16_t block_count_enable : 1;
        uint16_t auto_cmd12_enable : 2;
        uint16_t data_transfer_direction_select : 1;
        uint16_t multi_block_select : 1;
        uint16_t response_type_r1r5 : 1;
        uint16_t response_error_check_enable : 1;
        uint16_t response_interrupt_disable : 1;
        uint16_t : 7;
    };
    uint16_t value;
};

static_assert(sizeof(sdhci_trnmod) == 0x2);

union sdhci_blksize {
    struct {
        uint16_t transfer_block_size : 12;
        uint16_t sdma_buffer_boundary : 3;
        uint16_t : 1;
    };
    uint16_t value;
};

static_assert(sizeof(sdhci_blksize) == 0x2);

struct adma2_descriptor64 {
    uint32_t valid : 1;
    uint32_t end : 1;
    uint32_t interrupt : 1;
    uint32_t zero1 : 1;
    uint32_t act1 : 1;
    uint32_t act2 : 1;
    uint32_t zero2 : 10;
    uint32_t length : 16;
    uint64_t address;
} __attribute((packed));

static_assert(sizeof(adma2_descriptor64) == 0xc);

union acmd41_arg {
    struct {
        uint32_t voltage_window : 24;
        uint32_t s18r : 1;
        uint32_t : 3;
        uint32_t xpc : 1;
        uint32_t : 1;
        uint32_t hcs : 1;
        uint32_t : 1;
    };
    uint32_t value;
};

static_assert(sizeof(acmd41_arg) == 0x4);

union sd_ocr {
    struct {
        uint32_t voltage_window : 24;
        uint32_t s18a : 1;
        uint32_t : 2;
        uint32_t co2t : 1;
        uint32_t : 1;
        uint32_t uhsii_card_status : 1;
        uint32_t ccs : 1;
        uint32_t power_up_status : 1;
    };
    uint32_t value;
};

static_assert(sizeof(sd_ocr) == 0x4);

struct sd_scr {
    union {
        struct {
            uint32_t sd_spec : 4;
            uint32_t scr_structure : 4;
            uint32_t sd_bus_widths : 4;
            uint32_t sd_security : 3;
            uint32_t data_stat_after_erase : 1;
            uint32_t sd_specx1 : 2;
            uint32_t sd_spec4 : 1;
            uint32_t ex_security : 4;
            uint32_t sd_spec3 : 1;
            uint32_t cmd_speed_class_control : 1;
            uint32_t cmd_set_block_count : 1;
            uint32_t cmd_extension_register_single_block : 1;
            uint32_t cmd_extension_register_multiblock : 1;
            uint32_t cmd_secure_receive_and_send : 1;
            uint32_t : 1;
            uint32_t sd_specx2 : 2;
        };
        uint32_t high;
    };
    uint32_t low; // reserved for manufacturer usage
};

static_assert(sizeof(sd_scr) == 0x8);

union sd_cid {
    struct {
        uint16_t MDT : 12;
        uint16_t : 4;
        uint32_t PSN;
        uint8_t PRV;
        char PNM[5];
        uint16_t OID;
        uint8_t MID;
        uint8_t padding;
    } __attribute((packed));
    struct {
        uint32_t val1;
        uint32_t val2;
        uint32_t val3;
        uint32_t val4;
    };
};

static_assert(sizeof(sd_cid) == 0x10);

union sd_csd {
    struct {
        __uint128_t : 1;
        __uint128_t wp_upc : 1;
        __uint128_t file_format : 2;
        __uint128_t tmp_write_protect : 1;
        __uint128_t perm_write_protect : 1;
        __uint128_t copy : 1;
        __uint128_t file_format_grp : 1;
        __uint128_t : 5;
        __uint128_t write_bl_partial : 1;
        __uint128_t write_bl_len : 4;
        __uint128_t r2w_factor : 3;
        __uint128_t : 2;
        __uint128_t wp_grp_enable : 1;
        __uint128_t wp_grp_size : 7;
        __uint128_t sector_size : 7;
        __uint128_t erase_blk_en : 1;
        __uint128_t c_size_mult : 3;
        __uint128_t vdd_w_curr_max : 3;
        __uint128_t vdd_w_curr_min : 3;
        __uint128_t vdd_r_curr_max : 3;
        __uint128_t vdd_r_curr_min : 3;
        __uint128_t c_size : 12;
        __uint128_t : 2;
        __uint128_t dsr_imp : 1;
        __uint128_t read_blk_misalign : 1;
        __uint128_t write_blk_misalign : 1;
        __uint128_t read_bl_partial : 1;
        __uint128_t read_bl_len : 4;
        __uint128_t ccc : 12;
        __uint128_t tran_speed : 8;
        __uint128_t nsac : 8;
        __uint128_t taac : 8;
        __uint128_t : 6;
        __uint128_t csd_structure : 2;
        __uint128_t : 8;
    } v1 __attribute((packed));
    struct {
        __uint128_t : 1;
        __uint128_t wp_upc : 1;
        __uint128_t file_format : 2;
        __uint128_t tmp_write_protect : 1;
        __uint128_t perm_write_protect : 1;
        __uint128_t copy : 1;
        __uint128_t file_format_grp : 1;
        __uint128_t : 5;
        __uint128_t write_bl_partial : 1;
        __uint128_t write_bl_len : 4;
        __uint128_t r2w_factor : 3;
        __uint128_t : 2;
        __uint128_t wp_grp_enable : 1;
        __uint128_t wp_grp_size : 7;
        __uint128_t sector_size : 7;
        __uint128_t erase_blk_en : 1;
        __uint128_t : 1;
        __uint128_t c_size : 28;
        __uint128_t dsr_imp : 1;
        __uint128_t read_blk_misalign : 1;
        __uint128_t write_blk_misalign : 1;
        __uint128_t read_bl_partial : 1;
        __uint128_t read_bl_len : 4;
        __uint128_t ccc : 12;
        __uint128_t tran_speed : 8;
        __uint128_t nsac : 8;
        __uint128_t taac : 8;
        __uint128_t : 6;
        __uint128_t csd_structure : 2;
        __uint128_t : 8;
    } v2 __attribute((packed));
    struct {
        uint32_t val1;
        uint32_t val2;
        uint32_t val3;
        uint32_t val4;
    };
};

static_assert(sizeof(sd_csd) == 0x10);

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

enum class sd_app_cmd : uint16_t {
    SET_BUS_WIDTH = 6,
    SD_STATUS = 13,
    SEND_NUM_WR_BLOCKS = 22,
    SET_WR_BLK_ERASE_COUNT = 23,
    SD_SEND_OP_COND = 41,
    SET_CLR_CARD_DETECT = 42,
    SEND_SCR = 51,
    SECURE_RECEIVE = 53,
    SECURE_SEND = 54,
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
    sdhci_device(span<volatile uint8_t> mmio, uint32_t irq,
                 bool level_sensitive);
    bool isr();

    bool initialized = false;
    list_entry slots = { &slots, &slots };
};

enum class sdhci_card_type {
    none,
    normal_capacity,
    high_capacity,
};

class sdhci_slot {
public:
    sdhci_slot(sdhci_device& d, volatile sdhci_regs& regs);
    bool isr();
    void thread();
    void card_removal();
    void card_insertion();
    bool set_clock(unsigned int freq);
    void set_power();
    bool send_command(enum sd_cmd cmd, uint32_t arg);
    bool send_app_command(enum sd_app_cmd cmd, uint32_t arg);
    bool read_scr();

    list_entry le;
    sdhci_device& d;
    uint16_t hcver;
    volatile sdhci_regs& regs;
    object_ref thread_event, command_complete_event, dma_interrupt_event,
               buffer_read_ready_event;
    bool card_removal_pending = false;
    bool card_insertion_pending = false;
    unsigned int base_clock_frequency;
    bool initialized = false;
    sd_cid cid;
    uint16_t rca;
    sd_csd csd;
    uint64_t size;
    sdhci_errint last_error;
    sdhci_capab capab;
    enum sdhci_card_type type = sdhci_card_type::none;
    sd_scr scr;
    rwsem lock;
    unsigned int slot_num;
};

class sd_volume : public volume {
public:
    sd_volume(sdhci_slot& slot) : slot(slot) { }

    void release() override {
        if (--refcount == 0)
            delete this;
    }

    nt_status flush() override;
    nt_status dio_read(uint64_t page_num, uint64_t num_pages,
                       pfn_iterator& it, uint64_t& written_pages) override;
    nt_status dio_write(uint64_t page_num, uint64_t num_pages,
                        pfn_iterator& it, uint64_t& written_pages,
                        bool fua) override;

private:
    sdhci_slot& slot;
};

static bool init_sdhci_pci(const pci_device& dev);
static void found_sdhci_acpi(const acpi_device& dev);

static pci_class_recognizer mod_pci_class = {
    {}, pci_class::base_system_peripheral, (uint8_t)pci_base_system_peripheral::sdhci, true, init_sdhci_pci
};

static acpi_recognizer mod_pnp_hids = {
    {}, "80860F16", found_sdhci_acpi, nullptr,
};

static module_init module_init_sdhci([]() {
    insert_tail_list(pci_classes, mod_pci_class.le);
    insert_tail_list(pnp_hids, mod_pnp_hids.le);
});

static atomic<unsigned int> num_sdhci_slots = 0;

template<>
consteval bool format_spec_valid<enum sd_cmd>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, enum sd_cmd c) {
    switch (c) {
        using enum sd_cmd;

        case GO_IDLE_STATE:
            return do_format_arg(sp, "", "GO_IDLE_STATE");
        case ALL_SEND_CID:
            return do_format_arg(sp, "", "ALL_SEND_CID");
        case SEND_RELATIVE_ADDR:
            return do_format_arg(sp, "", "SEND_RELATIVE_ADDR");
        case SEND_DSR:
            return do_format_arg(sp, "", "SEND_DSR");
        case SWITCH_FUNC:
            return do_format_arg(sp, "", "SWITCH_FUNC");
        case SELECT_CARD:
            return do_format_arg(sp, "", "SELECT_CARD");
        case SEND_IF_COND:
            return do_format_arg(sp, "", "SEND_IF_COND");
        case SEND_CSD:
            return do_format_arg(sp, "", "SEND_CSD");
        case SEND_CID:
            return do_format_arg(sp, "", "SEND_CID");
        case VOLTAGE_SWITCH:
            return do_format_arg(sp, "", "VOLTAGE_SWITCH");
        case STOP_TRANSMISSION:
            return do_format_arg(sp, "", "STOP_TRANSMISSION");
        case SEND_STATUS:
            return do_format_arg(sp, "", "SEND_STATUS");
        case GO_INACTIVE_STATE:
            return do_format_arg(sp, "", "GO_INACTIVE_STATE");
        case SET_BLOCKLEN:
            return do_format_arg(sp, "", "SET_BLOCKLEN");
        case READ_SINGLE_BLOCK:
            return do_format_arg(sp, "", "READ_SINGLE_BLOCK");
        case READ_MULTIPLE_BLOCK:
            return do_format_arg(sp, "", "READ_MULTIPLE_BLOCK");
        case SEND_TUNING_BLOCK:
            return do_format_arg(sp, "", "SEND_TUNING_BLOCK");
        case SPEED_CLASS_CONTROL:
            return do_format_arg(sp, "", "SPEED_CLASS_CONTROL");
        case ADDRESS_EXTENSION:
            return do_format_arg(sp, "", "ADDRESS_EXTENSION");
        case SET_BLOCK_COUNT:
            return do_format_arg(sp, "", "SET_BLOCK_COUNT");
        case WRITE_SINGLE_BLOCK:
            return do_format_arg(sp, "", "WRITE_SINGLE_BLOCK");
        case WRITE_MULTIPLE_BLOCK:
            return do_format_arg(sp, "", "WRITE_MULTIPLE_BLOCK");
        case PROGRAM_CSD:
            return do_format_arg(sp, "", "PROGRAM_CSD");
        case SET_WRITE_PROT:
            return do_format_arg(sp, "", "SET_WRITE_PROT");
        case CLR_WRITE_PROT:
            return do_format_arg(sp, "", "CLR_WRITE_PROT");
        case SEND_WRITE_PROT:
            return do_format_arg(sp, "", "SEND_WRITE_PROT");
        case ERASE_WR_BLK_START:
            return do_format_arg(sp, "", "ERASE_WR_BLK_START");
        case ERASE_WR_BLK_END:
            return do_format_arg(sp, "", "ERASE_WR_BLK_END");
        case ERASE:
            return do_format_arg(sp, "", "ERASE");
        case SELECT_CARD_PARTITION:
            return do_format_arg(sp, "", "SELECT_CARD_PARTITION");
        case LOCK_UNLOCK:
            return do_format_arg(sp, "", "LOCK_UNLOCK");
        case Q_MANAGEMENT:
            return do_format_arg(sp, "", "Q_MANAGEMENT");
        case Q_TASK_INFO_A:
            return do_format_arg(sp, "", "Q_TASK_INFO_A");
        case Q_TASK_INFO_B:
            return do_format_arg(sp, "", "Q_TASK_INFO_B");
        case Q_RD_TASK:
            return do_format_arg(sp, "", "Q_RD_TASK");
        case Q_WR_TASK:
            return do_format_arg(sp, "", "Q_WR_TASK");
        case READ_EXTR_SINGLE:
            return do_format_arg(sp, "", "READ_EXTR_SINGLE");
        case WRITE_EXTR_SINGLE:
            return do_format_arg(sp, "", "WRITE_EXTR_SINGLE");
        case APP_CMD:
            return do_format_arg(sp, "", "APP_CMD");
        case GEN_CMD:
            return do_format_arg(sp, "", "GEN_CMD");
        case READ_EXTR_MULTI:
            return do_format_arg(sp, "", "READ_EXTR_MULTI");
        case WRITE_EXTR_MULTI:
            return do_format_arg(sp, "", "WRITE_EXTR_MULTI");
        default:
            return do_format_arg(sp, "", (unsigned int)c);
    }
}

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

    norintsts.value = regs.norintsts.value;

    if (norintsts.card_removal) {
        card_removal_pending = true;
        set_event(static_cast<event&>(thread_event.get()));

        norintsts.value = 0;
        norintsts.card_removal = 1;
        regs.norintsts.value = norintsts.value;

        return true;
    }

    if (norintsts.card_insertion) {
        card_insertion_pending = true;
        set_event(static_cast<event&>(thread_event.get()));

        norintsts.value = 0;
        norintsts.card_insertion = 1;
        regs.norintsts.value = norintsts.value;

        return true;
    }

    if (norintsts.command_complete) {
        set_event(static_cast<event&>(command_complete_event.get()));

        norintsts.value = 0;
        norintsts.command_complete = 1;
        regs.norintsts.value = norintsts.value;

        return true;
    }

    if (norintsts.dma_interrupt) {
        set_event(static_cast<event&>(dma_interrupt_event.get()));

        norintsts.value = 0;
        norintsts.dma_interrupt = 1;
        regs.norintsts.value = norintsts.value;

        return true;
    }

    if (norintsts.buffer_read_ready) {
        set_event(static_cast<event&>(buffer_read_ready_event.get()));

        norintsts.value = 0;
        norintsts.buffer_read_ready = 1;
        regs.norintsts.value = norintsts.value;

        return true;
    }

    if (norintsts.error_interrupt) {
        last_error.value = regs.errintsts.value;
        regs.errintsts.value = last_error.value; // clear error interrupts

        set_event(static_cast<event&>(command_complete_event.get()));
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

    sdhci_clkcon clkcon;

    clkcon.value = 0;

    if ((hcver & 0xff) >= 2) { // SDHCI 3.00 or above
        // FIXME - check for optional programmable clock mode

        if (freq >= base_clock_frequency)
            div = 0;
        else {
            // version 3 divisors must be multiple of 2
            for (div = 1; div < 0x3ff; div++) {
                if ((base_clock_frequency / (2 * div)) <= freq)
                    break;
            }
        }

        clkcon.sdclk_frequency_select = div & 0xff;
        clkcon.sdclk_frequency_select_upper = (div & 0x300) >> 8;
    } else {
        // version 2 divisors must be power of 2

        for (div = 1; div < 256; div <<= 1) {
            if ((base_clock_frequency / div) <= freq)
                break;
        }

        clkcon.sdclk_frequency_select = (uint8_t)(div >> 1);
    }

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

bool sdhci_slot::send_command(enum sd_cmd cmd, uint32_t arg) {
    reset_event(static_cast<event&>(command_complete_event.get()));
    regs.norintstsen.command_complete = 1;

    response_type type;

    switch (cmd) {
        using enum sd_cmd;

        case GO_IDLE_STATE:
            type = response_type::none;
            break;

        case ALL_SEND_CID:
            type = response_type::R2;
            break;

        case SEND_RELATIVE_ADDR:
            type = response_type::R6;
            break;

        case SELECT_CARD:
            type = response_type::R1b;
            break;

        case SEND_IF_COND:
            type = response_type::R7;
            break;

        case SEND_CSD:
            type = response_type::R2;
            break;

        case SET_BLOCKLEN:
            type = response_type::R1;
            break;

        case READ_SINGLE_BLOCK:
            type = response_type::R1;
            break;

        case READ_MULTIPLE_BLOCK:
            type = response_type::R1;
            break;

        case SET_BLOCK_COUNT:
            type = response_type::R1;
            break;

        case APP_CMD:
            type = response_type::R1;
            break;

        default:
            print("sdhci_slot::send_command: unhandled command type {}\n", cmd);
            return false;
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

    switch (cmd) {
        case sd_cmd::READ_SINGLE_BLOCK:
        case sd_cmd::READ_MULTIPLE_BLOCK:
            cmdreg.data_present_select = 1;
        break;

        default:
            break;
    }

    last_error.value = 0;

    regs.argument = arg;
    regs.cmdreg = cmdreg.value;

    wait_for_object(command_complete_event.get(), false); // FIXME - timeout (still clear norintstsen flag)

    regs.norintstsen.command_complete = 0;

    if (last_error.value != 0) {
        print("sdhci_slot::send_command: error (errintsts = {:04x})\n",
              last_error.value);
        return false;
    }

    return true;
}

bool sdhci_slot::send_app_command(enum sd_app_cmd cmd, uint32_t arg) {
    if (!send_command(sd_cmd::APP_CMD, (uint32_t)(rca << 16)))
        return false;

    reset_event(static_cast<event&>(command_complete_event.get()));
    regs.norintstsen.command_complete = 1;

    response_type type;

    switch (cmd) {
        using enum sd_app_cmd;

        case SET_BUS_WIDTH:
            type = response_type::R1;
            break;

        case SD_SEND_OP_COND:
            type = response_type::R3;
            break;

        case SEND_SCR:
            type = response_type::R1;
            break;

        default:
            print("sdhci_slot::send_app_command: unhandled command type {}\n",
                  (uint16_t)cmd);
            return false;
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

    if (cmd == sd_app_cmd::SEND_SCR)
        cmdreg.data_present_select = 1;

    last_error.value = 0;

    regs.argument = arg;
    regs.cmdreg = cmdreg.value;

    wait_for_object(command_complete_event.get(), false); // FIXME - timeout (still clear norintstsen flag)

    regs.norintstsen.command_complete = 0;

    if (last_error.value != 0) {
        print("sdhci_slot::send_app_command: error (errintsts = {:04x})\n",
              last_error.value);
        return false;
    }

    return true;
}

void sdhci_slot::set_power() {
    sdhci_pwrcon pwrcon;

    // FIXME - choose voltage, and also send this to SEND_IF_COND?
    // FIXME - check capabilities to find which voltages are supported

    // FIXME - UHS-II?

    regs.pwrcon = 0;

    pwrcon.value = 0;
    pwrcon.vdd1_power = 1;
    pwrcon.vdd1_voltage = 7; // 3.3V

    regs.pwrcon = pwrcon.value;
}

bool sdhci_slot::read_scr() {
    regs.blksize = sizeof(sd_scr);
    regs.blkcnt = 1;

    reset_event(static_cast<event&>(buffer_read_ready_event.get()));
    regs.norintstsen.buffer_read_ready = 1;

    sdhci_trnmod trnmod;

    trnmod.value = 0;
    trnmod.dma_enable = 0;
    trnmod.data_transfer_direction_select = 1; // read
    trnmod.multi_block_select = 0; // single

    regs.trnmod = trnmod.value;

    if (!send_app_command(sd_app_cmd::SEND_SCR, 0)) {
        print("sdhci_slot::read_scr: send_app_command failed for SEND_SCR\n");
        regs.norintstsen.buffer_read_ready = 0;
        return false;
    }

    wait_for_object(buffer_read_ready_event.get(), false);

    scr.high = regs.bdata;

    wait_for_object(buffer_read_ready_event.get(), false);

    scr.low = regs.bdata;

    regs.norintstsen.buffer_read_ready = 0;

    if (scr.scr_structure != 0) {
        print("sdhci_slot::read_scr: SCR had scr_structure of {}, expected 0\n",
              scr.scr_structure);
        return false;
    }

    return true;
}

nt_status sd_volume::flush() {
    // flush not supported for SD cards, do nothing

    // FIXME - eMMC does support flush
    // FIXME - synchronize with slot's lock?

    return nt_status::SUCCESS;
}

static void flush_cache_from_descriptors(span<const adma2_descriptor64> descs,
                                         bool invalidate) {
    for (auto& d : descs) {
        flush_cache(span((uint8_t*)phys_address(d.address), PAGE_SIZE), invalidate);
    }
}

nt_status sd_volume::dio_read(uint64_t page_num, uint64_t num_pages,
                              pfn_iterator& it, uint64_t& written_pages) {
    uint64_t address = page_num << PAGE_SHIFT;

    // FIXME - handle dead volumes
    // FIXME - handle reads beyond end of volume
    // FIXME - what if volume not multiple of PAGE_SIZE?

    // limit to one page's worth of descriptors
    static const size_t MAX_PAGES = PAGE_SIZE / sizeof(adma2_descriptor64);
    static const size_t BLOCKS_PER_PAGE = PAGE_SIZE / BLOCK_SIZE;

    // SDHCI can only handle 65535 blocks at once
    static_assert(MAX_PAGES * BLOCKS_PER_PAGE < 0x10000);

    num_pages = min(PAGE_SIZE / sizeof(adma2_descriptor64), num_pages);

    lock_guard lg(slot.lock);

    // prepare ADMA2 data structure

    auto adma_page = allocate_folio(0);
    if (!adma_page) {
        print("allocate_folio failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto descs = span((adma2_descriptor64*)phys_address(adma_page),
                      PAGE_SIZE / sizeof(adma2_descriptor64));

    for (uint64_t i = 0; i < num_pages; i++) {
        if (i != 0)
            it++;

        descs[i].valid = 1;

        if (i == num_pages - 1) {
            descs[i].end = 1;
            descs[i].interrupt = 1;
        } else {
            descs[i].end = 0;
            descs[i].interrupt = 0;
        }

        descs[i].zero1 = 0;
        descs[i].act1 = 0;
        descs[i].act2 = 1;
        descs[i].zero2 = 0;
        descs[i].length = PAGE_SIZE == 65536 ? 0 : PAGE_SIZE;
        descs[i].address = *it;
    }

    ::flush_cache(span((uint8_t*)descs.data(), num_pages * sizeof(adma2_descriptor64)),
                  false);

    slot.regs.admasysaddr[0] = (uint32_t)adma_page;
    slot.regs.admasysaddr[1] = (uint32_t)(adma_page >> 32);

    // set transfer mode register

    sdhci_trnmod trnmod;

    trnmod.value = 0;
    trnmod.dma_enable = 1;
    trnmod.data_transfer_direction_select = 1; // read
    trnmod.multi_block_select = 1; // multiple
    trnmod.block_count_enable = 1;

    // enable auto-CMD12 if we're not calling SET_BLOCK_COUNT
    trnmod.auto_cmd12_enable = slot.scr.cmd_set_block_count ? 0 : 1;

    auto num_blocks = (uint16_t)(num_pages * BLOCKS_PER_PAGE);

    slot.regs.trnmod = trnmod.value;
    slot.regs.blkcnt = num_blocks;

    flush_cache_from_descriptors(span(descs.data(), num_pages), false);

    reset_event(static_cast<event&>(slot.dma_interrupt_event.get()));
    slot.regs.norintstsen.dma_interrupt = 1;

    uint32_t arg;

    if (slot.type == sdhci_card_type::normal_capacity)
        arg = (uint32_t)address;
    else
        arg = (uint32_t)(address / BLOCK_SIZE);

    if (slot.scr.cmd_set_block_count) {
        if (!slot.send_command(sd_cmd::SET_BLOCK_COUNT, num_blocks)) {
            print("sd_volume::dio_read: send_command failed for SET_BLOCK_COUNT\n");
            flush_cache_from_descriptors(span(descs.data(), num_pages), true);
            free_folio(adma_page);
            slot.regs.norintstsen.dma_interrupt = 0;
            return nt_status::IO_DEVICE_ERROR; // FIXME
        }
    }

    if (!slot.send_command(sd_cmd::READ_MULTIPLE_BLOCK, arg)) {
        print("sd_volume::dio_read: send_command failed for READ_MULTIPLE_BLOCK\n");
        flush_cache_from_descriptors(span(descs.data(), num_pages), true);
        free_folio(adma_page);
        slot.regs.norintstsen.dma_interrupt = 0;
        return nt_status::IO_DEVICE_ERROR; // FIXME
    }

    wait_for_object(slot.dma_interrupt_event.get(), false);

    slot.regs.norintstsen.dma_interrupt = 0;

    flush_cache_from_descriptors(span(descs.data(), num_pages), true);

    free_folio(adma_page);

    written_pages = num_pages;

    return nt_status::SUCCESS;
}

nt_status sd_volume::dio_write(uint64_t page_num, uint64_t num_pages,
                               pfn_iterator& /*it*/, uint64_t& /*written_pages*/,
                               bool /*fua*/) {
    print("FIXME: sd_volume::dio_write({:x}, {:x})\n", page_num, num_pages);

    // FIXME

    return nt_status::MEDIA_WRITE_PROTECTED;
}

void sdhci_slot::card_insertion() {
    // FIXME - check card is actually inserted
    // FIXME - what do we do if we think there's a card already there?

    // set to max timeout (TMCLK x 2^27)
    regs.timeoutcon = 0xe;

    if (!set_clock(400000)) {
        print("sdhci_slot::card_insertion: set_clock failed\n");
        return;
    }

    {
        sdhci_hostctl hostctl;

        hostctl.value = regs.hostctl;
        hostctl.data_transfer_width = 0; // 1-bit mode

        regs.hostctl = hostctl.value;
    }

    set_power();

    if (!send_command(sd_cmd::GO_IDLE_STATE, 0)) {
        print("sdhci_slot::card_insertion: GO_IDLE_STATE failed\n");
        return;
    }

    bool f8;

    // FIXME - distinguish between errors and command getting ignored

    // old-style SD cards ignore this command
    if (send_command(sd_cmd::SEND_IF_COND, 0x1aa))
        f8 = true;
    else
        f8 = false;

    // FIXME - check response (should match argument)
    // FIXME - specs say we should try SEND_IF_COND once more if it fails

    // send ACMD41

    rca = 0;

    if (!send_app_command(sd_app_cmd::SD_SEND_OP_COND, 0)) {
        print("sdhci_slot::card_insertion: SD_SEND_OP_COND failed\n");
        return;
    }

    sd_ocr ocr;

    ocr.value = regs.rspreg0;

    acmd41_arg arg;

    arg.value = 0;

    arg.voltage_window = ocr.voltage_window;
    arg.hcs = f8 ? 1 : 0;

    unsigned int times = 20;

    while (true) {
        // send ACMD41 again
        if (!send_app_command(sd_app_cmd::SD_SEND_OP_COND, arg.value)) {
            print("sdhci_slot::card_insertion: SD_SEND_OP_COND failed\n");
            return;
        }

        ocr.value = regs.rspreg0;

        if (ocr.power_up_status)
            break;

        if (times == 0) {
            print("sdhci_slot::card_insertion: timed out doing SD_SEND_OP_COND\n");
            return;
        }

        times--;

        {
            auto tim_ref = create_timer(false);
            if (!tim_ref) {
                print("sdhci_slot::card_insertion: create_timer failed\n");
                return;
            }

            auto& tim = static_cast<timer&>(tim_ref.get());

            auto status = tim.set(50 * -ONE_MS_TICKS, nullptr, nullptr,
                                  false, 1, nullptr);
            if (!success(status)) {
                print("sdhci_slot::card_insertion: timer::set returned {}\n", status);
                return;
            }

            wait_for_object(tim, true); // wait 50ms
        }
    }

    if (f8) {
        if (ocr.ccs)
            type = sdhci_card_type::high_capacity;
        else
            type = sdhci_card_type::normal_capacity;
    } else
        type = sdhci_card_type::normal_capacity;

    // get CID

    if (!send_command(sd_cmd::ALL_SEND_CID, 0)) {
        print("sdhci_slot::card_insertion: ALL_SEND_CID failed\n");
        return;
    }

    cid.val1 = regs.rspreg0;
    cid.val2 = regs.rspreg1;
    cid.val3 = regs.rspreg2;
    cid.val4 = regs.rspreg3;

    // get RCA

    if (!send_command(sd_cmd::SEND_RELATIVE_ADDR, 0)) {
        print("sdhci_slot::card_insertion: SEND_RELATIVE_ADDR failed\n");
        return;
    }

    // FIXME - specs say we should repeat SEND_RELATIVE_ADDR if rca is 0
    rca = (uint16_t)(regs.rspreg0 >> 16);

    if (!set_clock(25000000)) {
        print("sdhci_slot::card_insertion: set_clock failed\n");
        return;
    }

    if (!send_command(sd_cmd::SEND_CSD, (uint32_t)(rca << 16))) {
        print("sdhci_slot::card_insertion: SEND_CSD failed\n");
        return;
    }

    csd.val1 = regs.rspreg0;
    csd.val2 = regs.rspreg1;
    csd.val3 = regs.rspreg2;
    csd.val4 = regs.rspreg3;

    switch (csd.v1.csd_structure) {
        case 0:
            size = ((uint64_t)csd.v1.c_size + 1) << (csd.v1.c_size_mult + 2 + csd.v1.read_bl_len);
            break;

        case 1:
            size = ((uint64_t)csd.v2.c_size & 0x3fffff) << 19; // units of 512 KB
            break;

        case 2:
            size = (uint64_t)csd.v2.c_size << 19; // units of 512 KB
            break;

        default:
            print("sdhci_slot::card_insertion: unhandled CSD type {}\n",
                  csd.v1.csd_structure);
            return;
    }

    if (!send_command(sd_cmd::SELECT_CARD, (uint32_t)(rca << 16))) {
        print("sdhci_slot::card_insertion: SELECT_CARD failed\n");
        return;
    }

    if (type == sdhci_card_type::normal_capacity) {
        if (!send_command(sd_cmd::SET_BLOCKLEN, BLOCK_SIZE)) {
            print("sdhci_slot::card_insertion: send_command failed for SET_BLOCKLEN\n");
            return;
        }
    }

    if (!read_scr()) {
        print("sdhci_slot::card_insertion: read_scr failed\n");
        return;
    }

    if (scr.sd_bus_widths & 4) {
        if (!send_app_command(sd_app_cmd::SET_BUS_WIDTH, 2)) {
            print("sdhci_slot::card_insertion: send_app_command failed for SET_BUS_WIDTH\n");
            return;
        }

        sdhci_hostctl hostctl;

        hostctl.value = regs.hostctl;
        hostctl.data_transfer_width = 1;

        regs.hostctl = hostctl.value;
    }

    // set block size register

    sdhci_blksize blksize;

    blksize.value = regs.blksize;
    blksize.transfer_block_size = BLOCK_SIZE;

    regs.blksize = blksize.value;

    auto pv = new sd_volume(*this);
    if (!pv) {
        print("sdhci_slot::card_insertion: new failed\n");
        return;
    }

    object_ref ref{*pv};

    auto& v = *pv;

    {
        char s[50];
        auto sp = span(s, sizeof(s));

        sp = format(sp, "mmcblk{}", slot_num);

        auto sv = string_view(s, (size_t)(sp.data() - s));

        v.volume_name_len = sv.size();
        v.volume_name = (char*)malloc(sv.size());
        if (!v.volume_name) {
            print("malloc failed\n");
            return;
        }

        memcpy(v.volume_name, sv.data(), sv.size());

        auto status = name_object(v, sv);
        if (!success(status)) {
            print("name_object returned {}\n", status);
            return;
        }
    }

    v.sector_size = BLOCK_SIZE;
    v.size = size;

    // FIXME - include SD card spec version
    print("found /dev/{}: block size {}, capacity {}\n", string_view(v.volume_name, v.volume_name_len),
          byte_size(v.sector_size), byte_size(v.size));

    v.probe();
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
    hcver = *(volatile uint16_t*)&regs.hcver;

    print("SDHCI version {}\n", spec_version_to_string((uint8_t)(hcver & 0xff)));

    // reset

    regs.swrst = 1;

    while (regs.swrst & 1) { } // FIXME - timeout

    // enable interrupts

    regs.norintsts.value = 0;
    regs.errintsts.value = 0;

    sdhci_norint norintstsen, norintsigen;

    norintstsen.value = 0;

    norintstsen.card_insertion = 1;
    norintstsen.card_removal = 1;

    regs.norintstsen.value = norintstsen.value;

    norintsigen.value = 0;

    norintsigen.card_insertion = 1;
    norintsigen.card_removal = 1;
    norintsigen.command_complete = 1;
    norintsigen.dma_interrupt = 1;
    norintsigen.buffer_read_ready = 1;

    regs.norintsigen.value = norintsigen.value;

    regs.errintstsen.value = 0xffff;
    regs.errintsigen.value = 0xffff;

    // get base clock frequency

    capab.low = regs.capab[0];
    capab.high = regs.capab[1];

    if (capab.base_clock_frequency == 0) {
        print("sdhci_slot::sdhci_slot: base clock frequency was 0\n");
        return;
    }

    base_clock_frequency = (unsigned int)capab.base_clock_frequency * 1000000;

    // check for ADMA2 and 64-bit DMA

    if (!capab.adma2_support) {
        print("sdhci_slot::sdhci_slot: controller does not support ADMA2\n");
        return;
    }

    if (!capab.v3_64bit_system_address_support) {
        print("sdhci_slot::sdhci_slot: controller does not support 64-bit DMA\n");
        return;
    }

    // enable ADMA2

    sdhci_hostctl hostctl;

    hostctl.value = regs.hostctl;

    if (capab.v3_64bit_system_address_support)
        hostctl.dma_select = 3; // 64-bit ADMA2
    else
        hostctl.dma_select = 2; // 32-bit ADMA2

    regs.hostctl = hostctl.value;

    slot_num = num_sdhci_slots.fetch_add(1);

    // start thread

    thread_event = create_event(true);
    command_complete_event = create_event(false);
    dma_interrupt_event = create_event(false);
    buffer_read_ready_event = create_event(false);

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

static bool init_sdhci_slot(sdhci_device& d, volatile sdhci_regs& regs) {
    auto s = new sdhci_slot(d, regs);
    if (!s) {
        print("new failed\n");
        return false;
    }

    if (!s->initialized) {
        print("sdhci_slot::sdhci_slot failed\n");
        delete s;
        return false;
    }

    insert_tail_list(d.slots, s->le);

    return true;
}

static bool sdhci_isr_func(void* ctx) {
    auto& dev = *(sdhci_device*)ctx;

    return dev.isr();
}

sdhci_device::sdhci_device(const pci_device& dev) {
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

        auto bar_num = (uint8_t)(slot_info_reg.first_bar_num + i);

        if (dev.bar[bar_num].size() < sizeof(sdhci_regs)) {
            print("sdhci_device::sdhci_device: BAR {} was {} bytes, expected {}\n",
                  bar_num, dev.bar[bar_num].size(), sizeof(sdhci_regs));
            continue;
        }

        volatile auto& regs = *(sdhci_regs*)dev.bar[bar_num].data();

        // FIXME - handle failure (what do we do if some slots fail and others don't?)
        init_sdhci_slot(*this, regs);
    }

    if (!add_irq_handler(*dev.irq, true, sdhci_isr_func, this)) {
        print("sdhci_device::sdhci_device: add_irq_handler failed\n");
        return;
    }

    initialized = true;
}

sdhci_device::sdhci_device(span<volatile uint8_t> mmio, uint32_t irq,
                           bool level_sensitive) {
    if (mmio.size() < sizeof(sdhci_regs)) {
        print("sdhci_device::sdhci_device: MMIO range was {} bytes, expected {}\n",
              mmio.size(), sizeof(sdhci_regs));
        return;
    }

    volatile auto& regs = *(sdhci_regs*)mmio.data();

    if (!init_sdhci_slot(*this, regs)) {
        print("init_sdhci_slot failed\n");
        return;
    }

    if (!add_irq_handler(irq, level_sensitive, sdhci_isr_func, this)) {
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

static bool init_sdhci_pci(const pci_device& dev) {
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

static void found_sdhci_acpi(const acpi_device& dev) {
    optional<pair<uint64_t, uint64_t>> address;
    optional<uint32_t> irq;
    bool level_sensitive;

    for (auto le = dev.resources.next; le != &dev.resources; le = le->next) {
        auto& r = containing_record<acpi_device_resource>(le);

        switch (r.type) {
            case acpi_device_resource_type::extended_irq: {
                auto& i = ((acpi_device_resource_extended_irq&)r).extended_irq;

                // FIXME - what are we supposed to do if multiple IRQs?

                if (!irq.has_value()) {
                    // FIXME - check entry only covers one IRQ?
                    irq = i.Interrupt;
                    level_sensitive = i.Triggering == ACPI_LEVEL_SENSITIVE;
                }

                break;
            }

            case acpi_device_resource_type::address: {
                auto& a = ((acpi_device_resource_address&)r).address;

                // FIXME - check memory is read-write?

                if (address.has_value()) {
                    print("found_sdhci_acpi: multiple address ranges provided\n");
                    return;
                }

                address = make_pair(a.Address.Minimum, a.Address.AddressLength);
                break;
            }

            case acpi_device_resource_type::fixed_memory32: {
                auto& m = ((acpi_device_resource_fixed_memory32&)r).fixed_memory32;

                // FIXME - check memory is read-write?

                if (address.has_value()) {
                    print("found_sdhci_acpi: multiple address ranges provided\n");
                    return;
                }

                address = make_pair(m.Address, m.AddressLength);
                break;
            }

            default:
                print("resource: {}\n", r.type);
                break;
        }
    }

    if (!address.has_value()) {
        print("found_sdhci_acpi: no memory address found\n");
        return;
    }

    if (!irq.has_value()) {
        print("found_sdhci_acpi: no IRQ found\n");
        return;
    }

    if (address->second == 0) {
        print("found_sdhci_acpi: memory address range has length of zero\n");
        return;
    }

    // map address range

    auto pa_start = address->first & ~(PAGE_SIZE - 1);
    auto pa_end = address->first + address->second;

    if (pa_end & (PAGE_SIZE - 1))
        pa_end = (pa_end & ~(PAGE_SIZE - 1)) + PAGE_SIZE;

    auto num_pages = (uint32_t)((pa_end - pa_start) >> PAGE_SHIFT);

    auto va = allocate_va(num_pages);
    if (!va) {
        print("allocate_va failed\n");
        return;
    }

    for (unsigned int i = 0; i < num_pages; i++) {
        if (!map_page(pa_start + (i << PAGE_SHIFT), va + (i << PAGE_SHIFT),
                      mapping_type::no_cache)) {
            print("map_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                unmap_page(va + (j << PAGE_SHIFT));
            }

            free_va(va, num_pages);
            return;
        }
    }

    auto mmio = span<volatile uint8_t>((volatile uint8_t*)(va + (address->first & (PAGE_SIZE - 1))),
                                       address->second);

    auto d = new sdhci_device(mmio, *irq, level_sensitive);
    if (!d || !d->initialized) {
        if (!d)
            print("new failed\n");
        else
            print("sdhci_device::sdhci_device failed\n");

        for (unsigned int i = 0; i < num_pages; i++) {
            unmap_page(va + (i << PAGE_SHIFT));
        }

        free_va(va, num_pages);

        if (d)
            delete d;

        return;
    }

    // FIXME - put in list somewhere?
}
