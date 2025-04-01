module;

#include "kernel.h"
#include "pci.h"
#include <span>
#include <bit>
#include "handle.h"
#include "spinlock.h"

export module nvme;

import print;
import config;
import mem;
import interrupts;
import event;
import semaphore;

using namespace std;

static const uint32_t VERSION_SUPPORTED = 0x00020200; // 2.2.0

static const uint16_t QUEUE_SIZE = 32;

static const uint8_t CSS_NCSS = 0x01;

static const uint32_t CC_AMS = 0x00003800;
static const uint32_t CC_MPS = 0x00000780;
static const unsigned int CC_MPS_SHIFT = 7;
static const uint32_t CC_CSS = 0x00000070;
static const uint32_t CC_EN = 0x00000001;

static const uint32_t CSTS_RDY = 0x00000001;

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

struct nvme_identify_data {
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
    uint8_t sqes;
    uint8_t cqes;
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

static_assert(sizeof(nvme_identify_data) == 4096);

class nvme_queue {
public:
    nvme_queue() {
        sem = create_semaphore(QUEUE_SIZE);

        for (unsigned int i = 0; i < QUEUE_SIZE; i++) {
            event[i] = create_event(true);
        }
    }

    uint8_t get_slot();
    void release_slot(uint8_t slot);

    void* sq;
    void* cq;
    uint32_t sq_tail = 0;
    uint32_t cq_head = 0;
    object_ref sem;
    object_ref event[QUEUE_SIZE];
    spinlock slot_lock;
    uint32_t allocated_slots = 0;
};

class nvme {
public:
    nvme(const pci_device& dev);
    ~nvme();
    void admin_isr();

    bool initialized = false;

private:
    void reset_controller(uint64_t admin_sq_phys, uint64_t admin_cq_phys);
    bool identify_controller(nvme_queue& queue);

    volatile nvme_regs* regs;
    unsigned int doorbell_stride;
    uint64_t controller_info_phys = 0;
    uint64_t maximum_transfer_size;
    nvme_queue admin_queue;
};

static void nvme_admin_isr_func(void* ctx) {
    ((nvme*)ctx)->admin_isr();
}

nvme::nvme(const pci_device& dev) {
    auto bar = dev.bar[0];

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

    // enable MSI-X
    dev.msix->message_control |= 0x8000;

    reset_controller(admin_sq_phys, admin_cq_phys);

    admin_queue.sq = phys_address(admin_sq_phys);
    admin_queue.cq = phys_address(admin_cq_phys);

    // set up admin queue interrupt
    auto& vec = *(volatile msix_entry*)(dev.bar[dev.msix->bir].data() + (dev.msix->table_offset << 3));

    // FIXME - free this in dtor or if we fail here
    auto int_num = add_isr(nvme_admin_isr_func, this);
    if (int_num == 0) {
        print("add_isr failed\n");
        return;
    }

    vec.message_data = int_num;
    vec.message_address_high = 0;
    vec.message_address_low = apic_msi_address(0, false, false); // FIXME - spread out amongst processors
    vec.vector_control &= ~1; // unmask

    if (!identify_controller(admin_queue))
        return;

    initialized = true;
}

nvme::~nvme() {
    if (controller_info_phys)
        free_folio(controller_info_phys);
}

void nvme::reset_controller(uint64_t admin_sq_phys, uint64_t admin_cq_phys) {
    volatile uint32_t* cc = &regs->cc;
    volatile uint32_t* csts = &regs->csts;
    volatile uint32_t* aqa = &regs->aqa;

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

    regs->asq = admin_sq_phys;
    regs->acq = admin_cq_phys;

    // set admin queue size
    *aqa = ((QUEUE_SIZE - 1) << 16) | (QUEUE_SIZE - 1);

    // set EN to 1
    barrier();
    *cc |= CC_EN;

    // wait for CSTS.RDY to become 1
    while (!(*csts & CSTS_RDY)) { }
}

static constexpr unsigned int bytes_to_folio_order(size_t s) {
    uint32_t p = bytes_to_pages(s);

    if (p == 0)
        return 0;

    return 32 - countl_zero(p - 1);
}

uint8_t nvme_queue::get_slot() {
    wait_for_object(sem.get(), false);

    lock_guard l(slot_lock);

    uint32_t mask = 1;
    uint8_t c = 0;

    while (true) {
        if (!(allocated_slots & mask)) {
            allocated_slots |= mask;
            return c;
        }

        mask <<= 1;
        c++;
    }
}

void nvme_queue::release_slot(uint8_t slot) {
    {
        lock_guard l(slot_lock);

        allocated_slots &= ~(1 << (uint32_t)slot);
    }

    release_semaphore(static_cast<semaphore&>(sem.get()));
}

bool nvme::identify_controller(nvme_queue& queue) {
    auto slot = queue.get_slot();

    auto& sqe = *(nvme_identify_command*)((uint8_t*)queue.sq + (queue.sq_tail * sizeof(nvme_command)));

    auto p = allocate_folio(bytes_to_folio_order(sizeof(nvme_identify_data)));
    if (!p) {
        print("allocate_folio failed\n");
        queue.release_slot(slot);
        return false;
    }

    memset(&sqe, 0, sizeof(sqe));

    sqe.opc = NVME_ADMIN_OPCODE_IDENTIFY;
    sqe.fuse = 0;
    sqe.psdt = 0; // use PRPs
    sqe.cid = slot;
    sqe.nsid = 0;
    sqe.dptr.prp1 = p;
    sqe.cns = NVME_IDENTIFY_CNS_CONTROLLER;
    sqe.cntid = 0;
    sqe.cnssid = 0;
    sqe.csi = 0;

    queue.sq_tail++;

    auto sq_doorbell = (volatile uint32_t*)((uint8_t*)regs + 0x1000);

    *sq_doorbell = queue.sq_tail;

    if (queue.sq_tail == QUEUE_SIZE)
        queue.sq_tail = 0;

    wait_for_object(queue.event[slot].get(), false);

    auto& comp = *(nvme_completion*)((uint8_t*)queue.cq + (queue.cq_head * sizeof(nvme_completion)));

    auto status = comp.status;

    queue.cq_head++;

    if (queue.cq_head == QUEUE_SIZE)
        queue.cq_head = 0;

    auto cq_doorbell = (volatile uint32_t*)((uint8_t*)regs + 0x1000 + doorbell_stride);

    *cq_doorbell = queue.cq_head;

    queue.release_slot(slot);

    if (status != 0) {
        print("NVMe identify returned status {:x}\n", status);
        free_folio(p);
        return false;
    }

    controller_info_phys = p;

    const auto& controller_info = *(nvme_identify_data*)phys_address(controller_info_phys);

    string_view model_number(controller_info.mn, sizeof(controller_info.mn));

    while (!model_number.empty() && (model_number.back() == 0 || model_number.back() == ' ')) {
        model_number = string_view(model_number.data(), model_number.size() - 1);
    }

    print("Found NVMe device: {}\n", model_number);

    if (controller_info.cntrltype != 1) {
        print("not an I/O controller\n");
        return false;
    }

    uint8_t mpsmin = (*(&regs->cap) & 0xf000000000000) >> 48;

    if (controller_info.mdts == 0)
        maximum_transfer_size = 0;
    else
        maximum_transfer_size = 1 << (controller_info.mdts + mpsmin + 12);

    return true;
}

export void init_nvme(const pci_device& dev) {
    // FIXME - support MSI too?
    if (!dev.msix) {
        print("MSI-X not supported for NVMe device\n");
        return;
    }

    if (dev.msix->bir >= dev.bar.size()) {
        print("invalid value {} for NVMe MSI-X BIR\n", dev.msix->bir);
        return;
    }

    if ((size_t)(dev.msix->table_offset << 3) >= dev.bar[dev.msix->bir].size()) {
        print("NVMe MSI-X table offset {:x} overflowed BAR {}\n",
              dev.msix->table_offset << 3, dev.msix->bir);
        return;
    }

    nvme n(dev);

    if (!n.initialized)
        return;
}

void nvme::admin_isr() {
    auto& comp = *(nvme_completion*)((uint8_t*)admin_queue.cq + (admin_queue.cq_head * sizeof(nvme_completion)));

    set_event(static_cast<event&>(admin_queue.event[comp.cid].get()));
}
