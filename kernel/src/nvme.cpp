module;

#include "kernel.h"
#include "pci.h"
#include <span>
#include <bit>
#include "handle.h"
#include "spinlock.h"
#include "nvme.h"

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

static spinlock nvme_lock;
static list_entry nvme_controllers = { &nvme_controllers, &nvme_controllers };

class nvme_controller;

struct nvme_request {
    nvme_command sqe;
    nvme_completion cqe;
    object_ref event;
};

class nvme_queue {
public:
    nvme_queue(nvme_controller& controller) : controller(controller) {
        sem = create_semaphore(QUEUE_SIZE);

        for (unsigned int i = 0; i < QUEUE_SIZE; i++) {
            reqs[i] = nullptr;
        }
    }

    ~nvme_queue() {
        if (sq)
            free_folio(phys_memory_pfn(sq) << PAGE_SHIFT);

        if (cq)
            free_folio(phys_memory_pfn(cq) << PAGE_SHIFT);
    }

    nvme_controller& controller;
    uint8_t get_slot();
    void release_slot(uint8_t slot);
    void isr();
    void submit(nvme_request& req);

    void* sq = nullptr;
    void* cq = nullptr;
    uint32_t sq_tail = 0;
    uint32_t cq_head = 0;
    spinlock sq_lock, cq_lock;
    object_ref sem;
    spinlock slot_lock;
    uint32_t allocated_slots = 0;
    nvme_request* reqs[QUEUE_SIZE];
};

class nvme_controller {
public:
    nvme_controller(const pci_device& dev);
    ~nvme_controller();

    bool initialized = false;
    list_entry le;
    volatile nvme_regs* regs;
    unsigned int controller_num;
    unsigned int doorbell_stride;

private:
    void reset_controller(uint64_t admin_sq_phys, uint64_t admin_cq_phys);
    bool identify_controller(nvme_queue& queue);
    bool identify_namespaces(nvme_queue& queue);

    nvme_identify_controller_data* controller_info = nullptr;
    uint64_t maximum_transfer_size;
    nvme_queue admin_queue{*this};
};

static void nvme_isr_func(void* ctx) {
    ((nvme_queue*)ctx)->isr();
}

nvme_controller::nvme_controller(const pci_device& dev) {
    auto bar = dev.bar[0];

    if (bar.size() < sizeof(nvme_regs)) {
        print("init_nvme: BAR0 was {:x} bytes, expected at least {:x}\n",
              bar.size(), sizeof(nvme_regs));
        return;
    }

    controller_num = dev.num;

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
    auto int_num = add_isr(nvme_isr_func, &admin_queue);
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

    if (!identify_namespaces(admin_queue))
        return;

    initialized = true;
}

nvme_controller::~nvme_controller() {
    if (controller_info)
        free_folio(phys_memory_pfn(controller_info) << PAGE_SHIFT);
}

void nvme_controller::reset_controller(uint64_t admin_sq_phys, uint64_t admin_cq_phys) {
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

void nvme_queue::submit(nvme_request& req) {
    req.event = create_event(true);

    // FIXME - if get_slot would block, we should be queueing this on a waitlist instead

    auto slot = get_slot();

    req.sqe.cid = slot;

    {
        lock_guard lock(sq_lock);

        memcpy((uint8_t*)sq + (sq_tail * sizeof(nvme_command)),
               &req.sqe, sizeof(req.sqe));

        reqs[slot] = &req;

        barrier();

        sq_tail++;

        auto sq_doorbell = (volatile uint32_t*)((uint8_t*)controller.regs + 0x1000);

        *sq_doorbell = sq_tail;

        if (sq_tail == QUEUE_SIZE)
            sq_tail = 0;
    }
}

bool nvme_controller::identify_controller(nvme_queue& queue) {
    auto p = allocate_folio(bytes_to_folio_order(sizeof(nvme_identify_controller_data)));
    if (!p) {
        print("allocate_folio failed\n");
        return false;
    }

    nvme_request req;

    memset(&req.sqe, 0, sizeof(req.sqe));

    auto& sqe = *reinterpret_cast<nvme_identify_command*>(&req.sqe);

    sqe.opc = NVME_ADMIN_OPCODE_IDENTIFY;
    sqe.fuse = 0;
    sqe.psdt = 0; // use PRPs
    sqe.nsid = 0;
    sqe.dptr.prp1 = p;
    sqe.cns = NVME_IDENTIFY_CNS_CONTROLLER;
    sqe.cntid = 0;
    sqe.cnssid = 0;
    sqe.csi = 0;

    queue.submit(req);

    wait_for_object(req.event.get(), false);

    if (req.cqe.status != 0) {
        print("NVMe identify returned status {:x}\n", req.cqe.status);
        free_folio(p);
        return false;
    }

    controller_info = (nvme_identify_controller_data*)phys_address(p);

    string_view model_number(controller_info->mn, sizeof(controller_info->mn));

    while (!model_number.empty() && (model_number.back() == 0 || model_number.back() == ' ')) {
        model_number = string_view(model_number.data(), model_number.size() - 1);
    }

    print("Found NVMe device: {}\n", model_number);

    if (controller_info->cntrltype != 1) {
        print("not an I/O controller\n");
        return false;
    }

    uint8_t mpsmin = (*(&regs->cap) & 0xf000000000000) >> 48;

    if (controller_info->mdts == 0)
        maximum_transfer_size = 0;
    else
        maximum_transfer_size = 1 << (controller_info->mdts + mpsmin + 12);

    return true;
}

struct nvme_namespace_req {
    list_entry le;
    nvme_request req;
    uint64_t p;
};

bool nvme_controller::identify_namespaces(nvme_queue& queue) {
    auto p = allocate_folio(bytes_to_folio_order(MAX_NAMESPACES * sizeof(uint32_t)));
    if (!p) {
        print("allocate_folio failed\n");
        return false;
    }

    nvme_request req;

    memset(&req.sqe, 0, sizeof(req.sqe));

    auto& sqe = *reinterpret_cast<nvme_identify_command*>(&req.sqe);

    sqe.opc = NVME_ADMIN_OPCODE_IDENTIFY;
    sqe.fuse = 0;
    sqe.psdt = 0; // use PRPs
    sqe.nsid = 0;
    sqe.dptr.prp1 = p;
    sqe.cns = NVME_IDENTIFY_CNS_ACTIVE_NAMESPACES;
    sqe.cntid = 0;
    sqe.cnssid = 0;
    sqe.csi = 0;

    queue.submit(req);

    wait_for_object(req.event.get(), false);

    if (req.cqe.status != 0) {
        print("NVMe identify returned status {:x}\n", req.cqe.status);
        free_folio(p);
        return false;
    }

    auto ns_list = (uint32_t*)phys_address(p);
    list_entry list = { &list, &list };

    for (unsigned int i = 0; i < MAX_NAMESPACES; i++) {
        if (ns_list[i] == 0)
            break;

        auto req2 = new nvme_namespace_req;
        if (!req2) {
            print("malloc failed\n");
            free_folio(p);
            return false;
        }

        req2->p = allocate_folio(bytes_to_folio_order(sizeof(nvme_identify_namespace_data)));
        if (!req2->p) {
            print("allocate_folio failed\n");
            free_folio(req2->p);
            return false;
        }

        memset(&req2->req.sqe, 0, sizeof(req2->req.sqe));

        auto& sqe2 = *reinterpret_cast<nvme_identify_command*>(&req2->req.sqe);

        sqe2.opc = NVME_ADMIN_OPCODE_IDENTIFY;
        sqe2.fuse = 0;
        sqe2.psdt = 0; // use PRPs
        sqe2.nsid = ns_list[i];
        sqe2.dptr.prp1 = req2->p;
        sqe2.cns = NVME_IDENTIFY_CNS_NAMESPACE;
        sqe2.cntid = 0;
        sqe2.cnssid = 0;
        sqe2.csi = 0;

        insert_tail_list(list, req2->le);

        queue.submit(req2->req);
    }

    free_folio(p);

    while (!is_list_empty(list)) {
        auto& req2 = containing_record<nvme_namespace_req>(list.next);

        remove_entry_list(req2.le);

        wait_for_object(req2.req.event.get(), false);

        auto& nsinfo = *(nvme_identify_namespace_data*)phys_address(req2.p);

        auto fidxl = nsinfo.flbas & 0xf;

        auto num_lba_formats = min(nsinfo.nlbaf, (uint8_t)nsinfo.lbaf.size());

        if (num_lba_formats > 16)
            fidxl |= (nsinfo.flbas & 0x30) >> 1;

        if (fidxl >= num_lba_formats) {
            print("skipping NVMe namespace {}: FIDXL was {}, expected 0 to {}\n",
                  req2.req.sqe.nsid, fidxl, num_lba_formats - 1);
            free_folio(req2.p);
            delete &req2;
        }

        if (!(nsinfo.lbaf[fidxl] & 0xf0000)) {
            print("skipping NVMe namespace {}: LBADS was 0\n", req2.req.sqe.nsid);
            free_folio(req2.p);
            delete &req2;
        }

        auto block_size = 1 << ((nsinfo.lbaf[fidxl] & 0xf0000) >> 16);

        // FIXME - format byte counts nicely
        print("found /dev/nvme{}n{}: block size {}, capacity {}\n",
              controller_num, req2.req.sqe.nsid, block_size, nsinfo.nsze * block_size); // FIXME

        // FIXME - readonly if nsattr & 1

        free_folio(req2.p);
        delete &req2;
    }

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

    auto n = new nvme_controller(dev);
    if (!n) {
        print("malloc failed\n");
        return;
    }

    if (!n->initialized) {
        delete n;
        return;
    }

    {
        lock_guard lock(nvme_lock);
        insert_tail_list(nvme_controllers, n->le);
    }
}

void nvme_queue::isr() {
    auto& comp = *(nvme_completion*)((uint8_t*)cq + (cq_head * sizeof(nvme_completion)));

    if (comp.cid >= QUEUE_SIZE) {
        print("NVMe completion for unknown cid {:x}, ignoring\n", comp.cid);
        return;
    }

    auto& req = *reqs[comp.cid];

    {
        lock_guard lock(cq_lock);

        auto& comp = *(nvme_completion*)((uint8_t*)cq + (cq_head * sizeof(nvme_completion)));

        memcpy(&req.cqe, &comp, sizeof(req.cqe));

        reqs[comp.cid] = nullptr;

        barrier();

        cq_head++;

        if (cq_head == QUEUE_SIZE)
            cq_head = 0;

        auto cq_doorbell = (volatile uint32_t*)((uint8_t*)controller.regs + 0x1000 + controller.doorbell_stride);

        *cq_doorbell = cq_head;
    }

    release_slot((uint8_t)comp.cid);

    set_event(static_cast<::event&>(req.event.get()));
}
