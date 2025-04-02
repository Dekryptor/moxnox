#pragma once

#include <stdint.h>
#include <array>

static const unsigned int MAX_NAMESPACES = 1024;

static const uint8_t CSS_NCSS = 0x01;

static uint32_t CC_IOCQES_MASK = 0x00f00000;
static unsigned int CC_IOCQES_SHIFT = 20;
static uint32_t CC_IOSQES_MASK = 0x000f0000;
static unsigned int CC_IOSQES_SHIFT = 16;
static const uint32_t CC_AMS = 0x00003800;
static const uint32_t CC_MPS = 0x00000780;
static const unsigned int CC_MPS_SHIFT = 7;
static const uint32_t CC_CSS = 0x00000070;
static const uint32_t CC_EN = 0x00000001;

static const uint32_t CSTS_RDY = 0x00000001;

static const uint8_t NVME_ADMIN_OPCODE_CREATE_SQ = 0x01;
static const uint8_t NVME_ADMIN_OPCODE_CREATE_CQ = 0x05;
static const uint8_t NVME_ADMIN_OPCODE_IDENTIFY = 0x06;

static const uint8_t NVME_IO_OPCODE_READ = 0x02;

static const uint8_t NVME_IDENTIFY_CNS_NAMESPACE = 0x00;
static const uint8_t NVME_IDENTIFY_CNS_CONTROLLER = 0x01;
static const uint8_t NVME_IDENTIFY_CNS_ACTIVE_NAMESPACES = 0x02;

struct nvme_regs {
    uint64_t cap;
    uint32_t vs;
    uint32_t intms;
    uint32_t intmc;
    uint32_t cc;
    uint8_t reserved1[4];
    uint32_t csts;
    uint32_t nssr;
    uint32_t aqa;
    uint64_t asq;
    uint64_t acq;
    uint32_t cmbloc;
    uint32_t cmbsz;
    uint32_t bpinfo;
    uint32_t bprsel;
    uint64_t bpmbl;
    uint64_t cmbmsc;
    uint32_t cmbsts;
    uint32_t cmbebs;
    uint32_t cmbswtp;
    uint32_t nssd;
    uint32_t crto;
    uint8_t reserved2[0xd94];
    uint32_t pmrcap;
    uint32_t mcrctl;
    uint32_t pmrsts;
    uint32_t pmrebs;
    uint32_t pmrswtp;
    uint32_t pmrmscl;
    uint32_t pmrmscu;
    uint8_t reserved3[0x1e4];
} __attribute((packed));

static_assert(sizeof(nvme_regs) == 0x1000);

union nvme_dptr {
    struct {
        uint64_t prp1;
        uint64_t prp2;
    };
    // FIXME - SGL
};

struct nvme_command {
    struct {
        uint32_t opc : 8;
        uint32_t fuse : 2;
        uint32_t reserved : 4;
        uint32_t psdt : 2;
        uint32_t cid : 16;
    };
    uint32_t nsid;
    uint32_t cdw2;
    uint32_t cdw3;
    uint64_t mptr;
    nvme_dptr dptr;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute((packed));

static_assert(sizeof(nvme_command) == 64);

struct nvme_identify_command {
    struct {
        uint32_t opc : 8;
        uint32_t fuse : 2;
        uint32_t reserved : 4;
        uint32_t psdt : 2;
        uint32_t cid : 16;
    };
    uint32_t nsid;
    uint32_t reserved2[4];
    nvme_dptr dptr;
    uint8_t cns;
    uint8_t reserved3;
    uint16_t cntid;
    uint16_t cnssid;
    uint8_t reserved4;
    uint8_t csi;
    uint32_t reserved5[2];
    uint32_t uidx : 7;
    uint32_t reserved6 : 25;
    uint32_t reserved7;
} __attribute((packed));

static_assert(sizeof(nvme_identify_command) == 64);

struct nvme_create_cq_command {
    struct {
        uint32_t opc : 8;
        uint32_t fuse : 2;
        uint32_t reserved : 4;
        uint32_t psdt : 2;
        uint32_t cid : 16;
    };
    uint32_t nsid;
    uint32_t reserved2[4];
    nvme_dptr dptr;
    uint16_t qid;
    uint16_t qsize;
    uint32_t pc : 1;
    uint32_t ien : 1;
    uint32_t reserved3 : 14;
    uint32_t iv : 16;
    uint32_t reserved4[4];
} __attribute((packed));

static_assert(sizeof(nvme_create_cq_command) == 64);

struct nvme_create_sq_command {
    struct {
        uint32_t opc : 8;
        uint32_t fuse : 2;
        uint32_t reserved : 4;
        uint32_t psdt : 2;
        uint32_t cid : 16;
    };
    uint32_t nsid;
    uint32_t reserved2[4];
    nvme_dptr dptr;
    uint16_t qid;
    uint16_t qsize;
    uint32_t pc : 1;
    uint32_t qprio : 2;
    uint32_t reserved3 : 13;
    uint32_t cqid : 16;
    uint32_t nvmsetid : 16;
    uint32_t reserved4 : 16;
    uint32_t reserved5[3];
} __attribute((packed));

static_assert(sizeof(nvme_create_sq_command) == 64);

struct nvme_read_command {
    struct {
        uint32_t opc : 8;
        uint32_t fuse : 2;
        uint32_t reserved : 4;
        uint32_t psdt : 2;
        uint32_t cid : 16;
    };
    uint32_t nsid;
    uint64_t elbtu : 48;
    uint64_t reserved2 : 16;
    uint64_t mptr;
    nvme_dptr dptr;
    uint64_t slba;
    uint16_t nlb;
    uint16_t cetype : 4;
    uint16_t reserved3 : 4;
    uint16_t stc : 1;
    uint16_t reserved4 : 1;
    uint16_t prinfo : 4;
    uint16_t fua : 1;
    uint16_t lr : 1;
    union {
        struct {
            uint32_t dsm : 8;
            uint32_t reserved5 : 24;
        };
        struct {
            uint16_t cev;
            uint16_t reserved6;
        };
    };
    uint32_t elbtl;
    uint16_t elbat;
    uint16_t elbatm;
} __attribute((packed));

static_assert(sizeof(nvme_read_command) == 64);

struct nvme_completion {
    uint32_t dw0;
    uint32_t dw1;
    uint16_t sqhd;
    uint16_t sqid;
    uint16_t cid;
    uint16_t p : 1;
    uint16_t status : 15;
} __attribute((packed));

static_assert(sizeof(nvme_completion) == 16);

struct nvme_identify_controller_data {
    uint16_t vid;
    uint16_t ssvid;
    char sn[20];
    char mn[40];
    char fr[8];
    uint8_t rab;
    uint8_t ieee[3];
    uint8_t cmic;
    uint8_t mdts;
    uint16_t cntlid;
    uint32_t ver;
    uint32_t rtd3r;
    uint32_t rtd3e;
    uint32_t oaes;
    uint32_t ctratt;
    uint16_t rrls;
    uint8_t bpcap;
    uint8_t reserved1;
    uint32_t nssl;
    uint16_t reserved2;
    uint8_t plsi;
    uint8_t cntrltype;
    uint8_t fguid[16];
    uint16_t crdt1;
    uint16_t crdt2;
    uint16_t crdt3;
    uint8_t crcap;
    uint8_t reserved3[105];
    uint8_t reserved4[13];
    uint8_t nvmsr;
    uint8_t vwci;
    uint8_t mec;
    uint16_t oacs;
    uint8_t acl;
    uint8_t aerl;
    uint8_t frmw;
    uint8_t lpa;
    uint8_t elpe;
    uint8_t npss;
    uint8_t avscc;
    uint8_t apsta;
    uint16_t wctemp;
    uint16_t cctemp;
    uint16_t mtfa;
    uint32_t hmpre;
    uint32_t hmmin;
    uint8_t tnvmcap[16];
    uint8_t unvmcap[16];
    uint32_t rpmbs;
    uint16_t edstt;
    uint8_t dsto;
    uint8_t fwug;
    uint16_t kas;
    uint16_t hctma;
    uint16_t mntmt;
    uint16_t mxtmt;
    uint32_t sanicap;
    uint32_t hmminds;
    uint16_t hmmaxd;
    uint16_t nsetidmax;
    uint16_t endgidmax;
    uint8_t anatt;
    uint8_t anacap;
    uint32_t anagrpmax;
    uint32_t nanagrpid;
    uint32_t pels;
    uint16_t did;
    uint8_t kpioc;
    uint8_t reserved5;
    uint16_t mptfawr;
    uint8_t reserved6[6];
    uint8_t megcap[16];
    uint8_t tmpthha;
    uint8_t reserved7;
    uint16_t cqt;
    uint8_t reserved8[124];
    uint8_t minsqes : 4;
    uint8_t maxsqes : 4;
    uint8_t mincqes : 4;
    uint8_t maxcqes : 4;
    uint16_t maxcmd;
    uint32_t nn;
    uint16_t oncs;
    uint16_t fuses;
    uint8_t fna;
    uint8_t vwc;
    uint16_t awun;
    uint16_t awupf;
    uint8_t icsvscc;
    uint8_t nwpc;
    uint16_t acwu;
    uint16_t cdfs;
    uint32_t sgls;
    uint32_t mnan;
    uint8_t maxdna[16];
    uint32_t maxcna;
    uint32_t oaqd;
    uint8_t rhiri;
    uint8_t hirt;
    uint16_t cmmrtd;
    uint16_t nmmrtd;
    uint8_t minmrtg;
    uint8_t maxmrtg;
    uint8_t trattr;
    uint8_t reserved9;
    uint16_t mcudmq;
    uint16_t mnsudmq;
    uint16_t mcmr;
    uint16_t nmcmr;
    uint16_t mcdqpc;
    uint8_t reserved10[180];
    char subnqn[256];
    uint8_t reserved11[768];
    uint32_t ioccsz;
    uint32_t iorcsz;
    uint16_t icdoff;
    uint8_t fcatt;
    uint8_t msdbd;
    uint16_t ofcs;
    uint8_t dctype;
    uint8_t reserved12[241];
    uint8_t psd[32][32];
    uint8_t vs[1024];
} __attribute((packed));

static_assert(sizeof(nvme_identify_controller_data) == 4096);

struct nvme_identify_namespace_data {
    uint64_t nsze;
    uint64_t ncap;
    uint64_t nuse;
    uint8_t nsfeat;
    uint8_t nlbaf;
    uint8_t flbas;
    uint8_t mc;
    uint8_t dpc;
    uint8_t dps;
    uint8_t nmic;
    uint8_t rescap;
    uint8_t fpi;
    uint8_t dlfeat;
    uint16_t nawun;
    uint16_t nawupf;
    uint16_t nacwu;
    uint16_t nabsn;
    uint16_t nabo;
    uint16_t nabspf;
    uint16_t noiob;
    uint8_t nvmcap[16];
    uint16_t npwg;
    uint16_t npwa;
    uint16_t npdg;
    uint16_t npda;
    uint16_t nows;
    uint16_t mssrl;
    uint32_t mcl;
    uint8_t msrc;
    uint8_t kpios;
    uint8_t nulbaf;
    uint8_t reserved;
    uint32_t kpiodaag;
    uint32_t reserved2;
    uint32_t anagrpid;
    uint8_t reserved3[3];
    uint8_t nsattr;
    uint16_t nvmsetid;
    uint16_t endgid;
    uint8_t nguid[16];
    uint64_t eui64;
    std::array<uint32_t, 64> lbaf;
    uint8_t vs[3712];
} __attribute((packed));

static_assert(sizeof(nvme_identify_namespace_data) == 4096);
