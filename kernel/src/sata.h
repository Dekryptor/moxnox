#pragma once

#include "objects.h"
#include "spinlock.h"
#include "handle.h"

static const unsigned int SATA_NUM_CMD_TABLES = 32;

static const uint32_t ATA_DEV_BUSY = 0x80;
static const uint32_t ATA_DEV_DRQ = 0x08;

static const uint32_t HBA_PxIS_TFES = 0x40000000; // task file error status

static const uint32_t HBA_PxCMD_ST = 0x00000001;
static const uint32_t HBA_PxCMD_FRE = 0x00000010;
static const uint32_t HBA_PxCMD_FR = 0x00004000;
static const uint32_t HBA_PxCMD_CR = 0x00008000;

struct HBA_PORT {
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t rsv0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t rsv1[11];
    uint32_t vendor[4];
};

static_assert(sizeof(HBA_PORT) == 0x80);

static const uint32_t SATA_SIG_ATA = 0x00000101;
static const uint32_t SATA_SIG_ATAPI = 0xeb140101;

struct HBA_MEM {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t rsv[116];
    uint8_t vendor[96];
    HBA_PORT ports[1];
};

static_assert(offsetof(HBA_MEM, rsv) == 0x2c);
static_assert(offsetof(HBA_MEM, ports) == 0x100);

enum class FIS_TYPE : uint8_t {
    REG_H2D = 0x27,
    REG_D2H = 0x34,
    DMA_ACT = 0x39,
    DMA_SETUP = 0x41,
    DATA = 0x46,
    BIST = 0x58,
    PIO_SETUP = 0x5f,
    DEV_BITS = 0xa1
} FIS_TYPE;

enum class ata_command : uint8_t {
    READ_DMA_EX = 0x25,
    IDENTIFY = 0xec
};

struct FIS_REG_H2D {
    enum FIS_TYPE fis_type;
    uint8_t pmport : 4;
    uint8_t rsv0 : 3;
    uint8_t c : 1;
    enum ata_command command;
    uint8_t featurel;
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t featureh;
    uint8_t countl;
    uint8_t counth;
    uint8_t icc;
    uint8_t control;
    uint8_t rsv1[4];
};

static_assert(sizeof(FIS_REG_H2D) == 20);

struct HBA_CMD_HEADER {
    uint8_t cfl : 5;
    uint8_t a : 1;
    uint8_t w : 1;
    uint8_t p : 1;
    uint8_t r : 1;
    uint8_t b : 1;
    uint8_t c : 1;
    uint8_t rsv0 : 1;
    uint8_t pmp : 4;
    uint16_t prdtl;
    volatile uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t rsv1[4];
};

struct HBA_PRDT_ENTRY {
    uint32_t dba;
    uint32_t dbau;
    uint32_t rsv0;
    uint32_t dbc : 22;
    uint32_t rsv1 : 9;
    uint32_t i : 1;
};

static_assert(sizeof(HBA_PRDT_ENTRY) == 0x10);

static const unsigned int NUM_PRDT_ENTRIES = 8;

struct HBA_CMD_TBL {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t rsv[48];
    HBA_PRDT_ENTRY prdt_entry[NUM_PRDT_ENTRIES];
};

static_assert(sizeof(HBA_CMD_TBL) == 0x100);

struct ata_identify_device_block {
    struct {
        uint16_t reserved1 : 2;
        uint16_t incomplete_response : 1;
        uint16_t reserved2 : 12;
        uint16_t device_type : 1;
    } general_configuration;
    uint16_t reserved1;
    uint16_t specific_configuration;
    uint16_t reserved2[7];
    uint8_t serialnumber[20];
    uint16_t reserved3[3];
    uint8_t firmware_revision[8];
    uint8_t model_number[40];
    uint8_t maximum_block_transfer;
    uint8_t reserved4;
    struct {
        uint16_t feature_supported : 1;
        uint16_t reserved : 15;
    } trustedcomputing;
    struct {
        uint8_t long_phys_sect_align_error_reporting : 2;
        uint8_t reserved1 : 6;
        uint8_t dma_supported : 1;
        uint8_t lba_supported : 1;
        uint8_t iordy_disable : 1;
        uint8_t iordy_supported : 1;
        uint8_t reserved2 : 1;
        uint8_t standyby_timer_support : 1;
        uint8_t reserved3 : 2;
        uint16_t minimum_standby_timer_value : 1;
        uint16_t reserved4 : 15;
    } capabilities;
    uint16_t reserved5[2];
    uint16_t translation_fields_valid : 3;
    uint16_t reserved6 : 5;
    uint16_t freefall_control_sensitivity : 8;
    uint16_t reserved7[5];
    uint8_t current_multisector_setting;
    uint8_t multisector_setting_valid : 1;
    uint8_t reserved8 : 1;
    uint8_t sanitize_antifreeze_lock_ext_supported : 1;
    uint8_t sanitize_ata_standard : 1;
    uint8_t sanitize_feature_supported : 1;
    uint8_t crypto_scramble_ext_supported : 1;
    uint8_t overwrite_ext_supported : 1;
    uint8_t block_erase_ext_supported : 1;
    uint32_t user_addressable_sectors;
    uint16_t reserved9;
    uint16_t multiword_dma_support : 3;
    uint16_t reserved10 : 5;
    uint16_t multiword_dma_active : 3;
    uint16_t reserved11 : 5;
    uint16_t pio_modes : 2;
    uint16_t reserved12 : 6;
    uint16_t minimum_multiword_transfer_cycle_time;
    uint16_t recommended_multiword_transfer_cycle_time;
    uint16_t minimum_pio_cycle_time;
    uint16_t minimum_pio_cycle_time_iordy;
    struct {
        uint16_t reserved1 : 2;
        uint16_t nonvolatile_write_cache : 1;
        uint16_t extended_user_addressable_sectors_supported : 1;
        uint16_t device_encrypts_all_user_data : 1;
        uint16_t read_zero_after_trim_supported : 1;
        uint16_t optional_28bit_commands_supported : 1;
        uint16_t reserved2 : 1;
        uint16_t download_microcode_dma_supported : 1;
        uint16_t reserved3 : 1;
        uint16_t write_buffer_dma_supported : 1;
        uint16_t read_buffer_dma_supported : 1;
        uint16_t reserved4 : 1;
        uint16_t long_phys_sect_align_error_reporting_control_supported : 1;
        uint16_t deterministic_read_after_trim_supported : 1;
        uint16_t reserved5 : 1;
    } additional_supported;
    uint16_t reserved13[5];
    uint16_t queuedepth : 5;
    uint16_t reserved14 : 11;
    struct {
        uint16_t reserved1 : 1;
        uint16_t sata_gen1 : 1;
        uint16_t sata_gen2 : 1;
        uint16_t sata_gen3 : 1;
        uint16_t reserved2 : 4;
        uint16_t ncq : 1;
        uint16_t hipm : 1;
        uint16_t phy_events : 1;
        uint16_t ncq_unload : 1;
        uint16_t ncq_priority : 1;
        uint16_t host_auto_partial_to_slumber : 1;
        uint16_t device_auto_partial_to_slumber : 1;
        uint16_t read_log_dma_ext : 1;
        uint16_t reserved3 : 1;
        uint16_t signal_speed : 3;
        uint16_t ncq_streaming : 1;
        uint16_t ncq_queue_management : 1;
        uint16_t fpdma_queued : 1;
        uint16_t reserved4 : 9;
    } sata_capabilities;
    struct {
        uint16_t reserved1 : 1;
        uint16_t non_zero_offsets : 1;
        uint16_t dma_setup_autoactivation : 1;
        uint16_t initiating_power_management : 1;
        uint16_t in_order_data_delivery : 1;
        uint16_t hardware_feature_control : 1;
        uint16_t software_settings_preservation : 1;
        uint16_t ncq_autosense : 1;
        uint16_t reserved2 : 8;
    } sata_features_supported;
    struct {
        uint16_t reserved1 : 1;
        uint16_t non_zero_offsets : 1;
        uint16_t dma_setup_autoactivation : 1;
        uint16_t initiating_power_management : 1;
        uint16_t in_order_data_delivery : 1;
        uint16_t hardware_feature_control : 1;
        uint16_t software_settings_preservation : 1;
        uint16_t auto_partial_to_slumber : 1;
        uint16_t reserved2 : 8;
    } sata_features_enabled;
    uint16_t major_version;
    uint16_t minor_version;
    struct {
        uint16_t smart_commands : 1;
        uint16_t security_mode : 1;
        uint16_t reserved1 : 3;
        uint16_t volatile_write_cache : 1;
        uint16_t read_lookahead : 1;
        uint16_t reserved2 : 5;
        uint16_t write_buffer : 1;
        uint16_t read_buffer : 1;
        uint16_t nop : 1;
        uint16_t reserved3 : 1;
        uint16_t download_microcode : 1;
        uint16_t reserved4 : 2;
        uint16_t apm : 1;
        uint16_t reserved5 : 1;
        uint16_t puis : 1;
        uint16_t manual_power_up : 1;
        uint16_t reserved6 : 3;
        uint16_t big_lba : 1;
        uint16_t reserved7 : 2;
        uint16_t flush_cache_ext : 1;
        uint16_t reserved8 : 2;
        uint16_t smart_error_logging : 1;
        uint16_t smart_self_test : 1;
        uint16_t reserved9 : 2;
        uint16_t streaming_feature : 1;
        uint16_t gpl : 1;
        uint16_t write_fua : 1;
        uint16_t reserved10 : 6;
        uint16_t idle_with_unload_feature : 1;
        uint16_t reserved11 : 2;
    } command_set_support;
    struct { // 85
        uint16_t smart_commands : 1;
        uint16_t security_mode : 1;
        uint16_t reserved1 : 3;
        uint16_t volatile_write_cache : 1;
        uint16_t read_lookahead : 1;
        uint16_t reserved2 : 5;
        uint16_t write_buffer : 1;
        uint16_t read_buffer : 1;
        uint16_t nop : 1;
        uint16_t reserved3 : 1;
        uint16_t download_microcode : 1;
        uint16_t reserved4 : 2;
        uint16_t apm : 1;
        uint16_t reserved5 : 1;
        uint16_t puis : 1;
        uint16_t manual_power_up : 1;
        uint16_t reserved6 : 3;
        uint16_t big_lba : 1;
        uint16_t reserved7 : 1;
        uint16_t flush_cache : 1;
        uint16_t flush_cache_ext : 1;
        uint16_t reserved8 : 1;
        uint16_t command_set_ext_valid : 1;
        uint16_t smart_error_logging : 1;
        uint16_t smart_self_test : 1;
        uint16_t media_serial_number : 1;
        uint16_t reserved9 : 2;
        uint16_t gpl : 1;
        uint16_t write_fua : 1;
        uint16_t reserved10 : 6;
        uint16_t idle_with_unload_feature : 1;
        uint16_t reserved11 : 2;
    } command_set_active;
    uint16_t ultra_dma_support : 8;
    uint16_t ultra_dma_active : 8;
    struct {
        uint16_t time_required : 15;
        uint16_t extended_time_reported : 1;
    } normal_security_erase_unit;
    struct {
        uint16_t time_required : 15;
        uint16_t extended_time_reported : 1;
    } enhanced_security_erase_unit;
    uint16_t current_apm_level : 8;
    uint16_t reserved15 : 8;
    uint16_t master_password_identifier;
    uint16_t hardware_reset_result;
    uint16_t reserved16;
    uint16_t stream_minimum_request_size;
    uint16_t streaming_transfer_time_dma;
    uint16_t streaming_access_latency_dma_pio;
    uint32_t streaming_perf_granularity;
    uint64_t user_addressable_logical_sectors;
    uint16_t streaming_transfer_time;
    uint16_t data_set_management_max_blocks;
    struct {
        uint16_t logical_sectors_per_physical_sector : 4;
        uint16_t reserved1 : 8;
        uint16_t logical_sector_longer_than_256_words : 1;
        uint16_t multiple_logical_sectors_per_physical_sector : 1;
        uint16_t reserved2 : 2;
    } sector_size;
    uint16_t inter_seek_delay;
    uint16_t world_wide_name[4];
    uint16_t reserved17[5];
    uint32_t logical_sector_size;
    struct {
        uint16_t reserved1 : 1;
        uint16_t write_read_verify : 1;
        uint16_t write_uncorrectable_ext : 1;
        uint16_t read_write_log_dma_ext : 1;
        uint16_t download_microcode_mode3 : 1;
        uint16_t freefall_control : 1;
        uint16_t sense_data_reporting : 1;
        uint16_t epc_feature_set : 1;
        uint16_t reserved2 : 8;
    } command_set_support_ext;
    struct {
        uint16_t reserved1 : 1;
        uint16_t write_read_verify : 1;
        uint16_t write_uncorrectable_ext : 1;
        uint16_t read_write_log_dma_ext : 1;
        uint16_t download_microcode_mode3 : 1;
        uint16_t freefall_control : 1;
        uint16_t sense_data_reporting : 1;
        uint16_t epc_feature_set : 1;
        uint16_t reserved2 : 8;
    } command_set_active_ext;
    uint16_t reserved18[7];
    struct {
        uint16_t security_supported : 1;
        uint16_t security_enabled : 1;
        uint16_t security_locked : 1;
        uint16_t security_frozen : 1;
        uint16_t security_count_expired : 1;
        uint16_t enhanced_security_erase_supported : 1;
        uint16_t reserved1 : 2;
        uint16_t master_password_capability : 1;
        uint16_t reserved2 : 7;
    } security_status;
    uint16_t reserved19[39];
    uint16_t nominal_form_factor : 4;
    uint16_t reserved20 : 12;
    struct {
        uint16_t supports_trim : 1;
        uint16_t reserved : 15;
    } data_set_management_feature;
    uint16_t additional_product_identifier[4];
    uint16_t reserved21[2];
    uint16_t current_media_serial_number[30];
    struct {
        uint16_t supported : 1;
        uint16_t reserved1 : 1;
        uint16_t write_same_suported : 1;
        uint16_t error_recovery_control_supported : 1;
        uint16_t feature_control_suported : 1;
        uint16_t data_tables_suported : 1;
        uint16_t reserved2 : 10;
    } sct_command_transport;
    uint16_t reserved22[2];
    uint16_t logical_sector_offset : 14;
    uint16_t reserved23 : 2;
    uint32_t write_read_verify_sector_count_mode3_count;
    uint32_t write_read_verify_sector_count_mode2_count;
    uint16_t reserved24[3];
    uint16_t nominal_media_rotation_rate;
    uint16_t reserved25[2];
    uint16_t write_read_verify_sector_count_mode : 8;
    uint16_t reserved26 : 8;
    uint16_t reserved27;
    struct {
        uint16_t major_version : 7;
        uint16_t reserved : 5;
        uint16_t transport_type : 4;
    } transport_major_version;
    uint16_t transport_minor_version;
    uint16_t reserved28[6];
    uint64_t extended_number_of_user_addressable_sectors;
    uint16_t min_blocks_per_download_microcode;
    uint16_t max_blocks_per_download_microcode;
    uint16_t reserved29[19];
    uint16_t checksum_validity_indicator : 8;
    uint16_t checksum : 8;
} __attribute__((packed));

static_assert(sizeof(ata_identify_device_block) == 512);

class sata_device : public volume {
public:
    sata_device(volatile HBA_PORT& port);

    void release() override {
        if (--refcount == 0)
            delete this;
    }

    bool identify();
    uint8_t get_slot();
    void release_slot(uint8_t slot);
    size_t read(std::span<uint8_t> data, uint64_t offset) override;
    size_t read_uncached(std::span<uint8_t> data, uint64_t offset) override;
    void isr();

    list_entry le;
    volatile HBA_PORT& port;
    uint64_t clb, fb;
    std::array<HBA_CMD_TBL*, SATA_NUM_CMD_TABLES> cmd_tables;
    ata_identify_device_block identify_block;
    object_ref events[SATA_NUM_CMD_TABLES];
    char model_number_buf[40];
    std::string_view model_number;
    object_ref slot_semaphore;
    spinlock slot_lock, ci_lock;
    std::atomic<uint32_t> allocated_slots = 0;
    std::atomic<uint32_t> pending_slots = 0;
    unsigned int dev_num;
    bool valid = false;
};

class sata_card {
public:
    sata_card(const pci_device& dev) : dev(dev) { }
    void isr();

    const pci_device& dev;
    list_entry le;
    volatile HBA_MEM* abar;
    object_ref init_event;
    std::atomic<unsigned int> init_left = 0;
    spinlock port_list_lock;
    list_entry port_list = { &port_list, &port_list };
};
