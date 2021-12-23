#pragma once

static const unsigned int SATA_NUM_CMD_TABLES = 32;

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

struct FIS_REG_H2D {
    enum FIS_TYPE fis_type;
    uint8_t pmport : 4;
    uint8_t rsv0 : 3;
    uint8_t c : 1;
    uint8_t command;
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
} FIS_REG_H2D;

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

struct HBA_CMD_TBL {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t rsv[48];
    HBA_PRDT_ENTRY prdt_entry[8];
};

static_assert(sizeof(HBA_CMD_TBL) == 0x100);

class sata_device {
public:
    sata_device(volatile HBA_PORT& port);

    volatile HBA_PORT& port;
    uint64_t clb, fb;
    std::array<volatile HBA_CMD_TBL*, SATA_NUM_CMD_TABLES> cmd_tables;
};
