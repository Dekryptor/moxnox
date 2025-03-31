module;

#include "kernel.h"
#include "pci.h"

export module nvme;

import print;

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

export void init_nvme(const pci_device& dev) {
    volatile auto regs = (nvme_regs*)dev.bar[0].data();

    if (dev.bar[0].size() < sizeof(nvme_regs)) {
        print("init_nvme: BAR0 was {:x} bytes, expected at least {:x}\n",
              dev.bar[0].size(), sizeof(nvme_regs));
        return;
    }

    print("FIXME - init_nvme\n"); // FIXME
}
