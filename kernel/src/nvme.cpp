module;

#include "kernel.h"
#include "pci.h"
#include <span>

export module nvme;

import print;
import config;
import mem;

using namespace std;

static uint32_t VERSION_SUPPORTED = 0x00020200; // 2.2.0

static uint16_t ADMIN_QUEUE_SIZE = 32;

static uint8_t CSS_NCSS = 0x01;

static uint32_t CC_AMS = 0x00003800;
static uint32_t CC_MPS = 0x00000780;
static unsigned int CC_MPS_SHIFT = 7;
static uint32_t CC_CSS = 0x00000070;
static uint32_t CC_EN = 0x00000001;

static uint32_t CSTS_RDY = 0x00000001;

static const uint8_t NVME_ADMIN_OPCODE_IDENTIFY = 0x06;

static const uint8_t NVME_IDENTIFY_CNS_CONTROLLER = 0x01;

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
    uint32_t cdw15;
} __attribute((packed));

static_assert(sizeof(nvme_identify_command) == 64);

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

class nvme {
public:
    nvme(span<volatile uint8_t> bar);

    bool initialized = false;

private:
    void reset_controller(uint64_t admin_sq_phys, uint64_t admin_cq_phys);
    bool identify();

    volatile nvme_regs* regs;
    void* admin_sq;
    void* admin_cq;
    uint32_t admin_sq_tail = 0;
    uint32_t admin_cq_head = 0;
    unsigned int doorbell_stride;
};

nvme::nvme(span<volatile uint8_t> bar) {
    if (bar.size() < sizeof(nvme_regs)) {
        print("init_nvme: BAR0 was {:x} bytes, expected at least {:x}\n",
              bar.size(), sizeof(nvme_regs));
        return;
    }

    regs = (nvme_regs*)bar.data();

    uint32_t version = *(&regs->vs);

    if (version > VERSION_SUPPORTED) {
        auto version_major = version >> 16;
        auto version_minor = (version >> 8) & 0xff;
        auto version_tertiary = version & 0xff;

        print("unsupported NVMe version {}.{}.{}\n",
              version_major, version_minor, version_tertiary);
        return;
    }

    uint64_t cap = *(&regs->cap);
    uint8_t css = (cap & 0x1fe000000000) >> 37;

    if (!(css & CSS_NCSS)) {
        print("controller does not support NVMe command set\n");
        return;
    }

    uint8_t mpsmax = (cap & 0xf0000000000000) >> 52;
    uint8_t mpsmin = (cap & 0xf000000000000) >> 48;

    if ((unsigned int)(mpsmin + 12) > PAGE_SHIFT || (unsigned int)(mpsmax + 12) < PAGE_SHIFT) {
        print("NVMe controller does not support page size of {}\n", PAGE_SIZE);
        return;
    }

    doorbell_stride = 1 << (((cap & 0xf00000000) >> 32) + 2);

    auto admin_sq_phys = allocate_page();
    if (!admin_sq_phys) {
        print("allocate_page failed\n");
        return;
    }

    auto admin_cq_phys = allocate_page();
    if (!admin_cq_phys) {
        print("allocate_page failed\n");
        free_folio(admin_sq_phys);
        return;
    }

    reset_controller(admin_sq_phys, admin_cq_phys);

    admin_sq = phys_address(admin_sq_phys);
    admin_cq = phys_address(admin_cq_phys);

    if (!identify())
        return;

    initialized = true;
}

void nvme::reset_controller(uint64_t admin_sq_phys, uint64_t admin_cq_phys) {
    volatile uint32_t* cc = &regs->cc;
    volatile uint32_t* csts = &regs->csts;
    volatile uint32_t* aqa = &regs->aqa;
    volatile uint32_t* intms = &regs->intms;

    // set EN to 0
    *cc &= ~CC_EN;

    // wait for CSTS.RDY to become 0
    barrier();
    while (*csts & CSTS_RDY) { }

    // clear AMS bits (set to 000, round robin)
    *cc &= ~CC_AMS;

    // set MPS
    *cc &= ~CC_MPS;
    *cc |= (PAGE_SHIFT - 12) << CC_MPS_SHIFT;

    // clear CSS bits (set to 000, NVM commmand set)
    *cc &= ~CC_CSS;

    // disable interrupts for now
    *intms = 0xffffffff;

    regs->asq = admin_sq_phys;
    regs->acq = admin_cq_phys;

    // set admin queue size
    *aqa = ((ADMIN_QUEUE_SIZE - 1) << 16) | (ADMIN_QUEUE_SIZE - 1);

    // set EN to 1
    barrier();
    *cc |= CC_EN;

    // wait for CSTS.RDY to become 1
    while (!(*csts & CSTS_RDY)) { }
}

bool nvme::identify() {
    auto& sqe = *(nvme_identify_command*)((uint8_t*)admin_sq + (admin_sq_tail * sizeof(nvme_command)));

    auto p = allocate_page();
    if (!p) {
        print("allocate_page failed\n");
        return false;
    }

    memset(&sqe, 0, sizeof(sqe));

    sqe.opc = NVME_ADMIN_OPCODE_IDENTIFY;
    sqe.fuse = 0;
    sqe.psdt = 0; // use PRPs
    sqe.cid = 0;
    sqe.nsid = 0;
    sqe.dptr.prp1 = p;
    sqe.cns = NVME_IDENTIFY_CNS_CONTROLLER;
    sqe.cntid = 0;
    sqe.cnssid = 0;
    sqe.csi = 0;

    admin_sq_tail++;

    auto sq_doorbell = (volatile uint32_t*)((uint8_t*)regs + 0x1000);

    *sq_doorbell = admin_sq_tail;

    if (admin_sq_tail == ADMIN_QUEUE_SIZE)
        admin_sq_tail = 0;

    // FIXME - wait for completion

    // FIXME - get right entry
    auto& comp = *(nvme_completion*)((uint8_t*)admin_cq + (admin_cq_head * sizeof(nvme_completion)));

    auto status = comp.status;

    admin_cq_head++;

    if (admin_cq_head == ADMIN_QUEUE_SIZE)
        admin_cq_head = 0;

    auto cq_doorbell = (volatile uint32_t*)((uint8_t*)regs + 0x1000 + doorbell_stride);

    *cq_doorbell = admin_cq_head;

    if (status != 0) {
        print("NVMe identify returned status {:x}\n", status);
        free_folio(p);
        return false;
    }

    // FIXME - interpret returned data

    free_folio(p);

    return true;
}

export void init_nvme(const pci_device& dev) {
    print("FIXME - init_nvme\n"); // FIXME

    nvme n(dev.bar[0]);

    if (!n.initialized)
        return;
}
