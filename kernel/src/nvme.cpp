module;

#include "kernel.h"
#include "pci.h"
#include <span>

export module nvme;

import print;
import config;

using namespace std;

static uint32_t VERSION_SUPPORTED = 0x00020200; // 2.2.0

static uint8_t CSS_NCSS = 0x01;

static uint32_t CC_AMS = 0x00003800;
static uint32_t CC_MPS = 0x00000780;
static unsigned int CC_MPS_SHIFT = 7;
static uint32_t CC_CSS = 0x00000070;
static uint32_t CC_EN = 0x00000001;

static uint32_t CSTS_RDY = 0x00000001;

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

class nvme {
public:
    nvme(span<volatile uint8_t> bar);

    bool initialized = false;

private:
    void reset_controller();

    volatile nvme_regs* regs;
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

    reset_controller();

    initialized = true;
}

void nvme::reset_controller() {
    volatile uint32_t* cc = &regs->cc;
    volatile uint32_t* csts = &regs->csts;

    // set EN to 0
    *cc &= ~CC_EN;
    // wait for CSTS.RDY to become 0
    while (*csts & CSTS_RDY) { }

    // clear AMS bits (set to 000, round robin)
    *cc &= ~CC_AMS;

    // set MPS
    *cc &= ~CC_MPS;
    *cc |= (PAGE_SHIFT - 12) << CC_MPS_SHIFT;

    // clear CSS bits (set to 000, NVM commmand set)
    *cc &= ~CC_CSS;

    // set EN to 1
    *cc |= CC_EN;
    // wait for CSTS.RDY to become 1
    while (!(*csts & CSTS_RDY)) { }
}

export void init_nvme(const pci_device& dev) {
    nvme n(dev.bar[0]);

    print("FIXME - init_nvme\n"); // FIXME

    if (!n.initialized)
        return;
}
