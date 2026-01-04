module;

#include <span>
#include <string_view>
#include <shared_mutex>
#include <mutex>
#include "nvme.h"

export module nvme;

import nt;
import print;
import format;
import config;
import mem;
import interrupts;
import event;
import semaphore;
import volume;
import bsod;
import rwsem;
import pci;
import module_init;
import page_cache;
import scheduler;
import list;
import spinlock;
import handle;
import object_ref;
import crt;

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

static bool nvme_msix_isr_func(void* ctx);

class nvme_queue {
public:
    nvme_queue(nvme_controller& controller, unsigned int queue_id) : controller(controller), queue_id(queue_id) {
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

#ifdef __aarch64__
        if (isr_registered) {
            remove_irq_handler(irq_num, nvme_msix_isr_func, this);
            free_msi_irq(irq_num);
        }
#elifdef __x86_64__
        if (isr_registered)
            remove_isr(nvme_msix_isr_func, this);
#endif
    }

    nvme_controller& controller;
    unsigned int queue_id;
    uint8_t get_slot();
    void release_slot(uint8_t slot);
    bool isr();
    void submit(nvme_request& req);

    void* sq = nullptr;
    void* cq = nullptr;
    uint32_t sq_tail = 0;
    uint32_t cq_head = 0;
    spinlock sq_lock;
    object_ref sem;
    spinlock slot_lock;
    uint32_t allocated_slots = 0;
    nvme_request* reqs[QUEUE_SIZE];
    uint8_t exp_cq_p = 1;
    bool isr_registered = false;
#ifdef __aarch64__
    uint16_t irq_num;
#endif
};

class nvme_controller {
public:
    nvme_controller(const pci_device& dev);
    ~nvme_controller();
    bool isr();

    bool initialized = false;
    list_entry le;
    volatile nvme_regs* regs;
    unsigned int controller_num;
    unsigned int doorbell_stride;
    spinlock namespaces_lock;
    nvme_identify_controller_data* controller_info = nullptr;
    nvme_queue io_queue{*this, 1};
    uint64_t maximum_transfer_size;

private:
    void reset_controller(uint64_t admin_sq_phys, uint64_t admin_cq_phys);
    bool identify_controller(nvme_queue& queue);
    bool create_io_queues(const pci_device& dev, nvme_queue& queue);
    bool identify_namespaces(nvme_queue& queue);

    nvme_queue admin_queue{*this, 0};
    list_entry namespaces = { &namespaces, &namespaces };
};

class nvme_namespace : public volume {
public:
    nvme_namespace(nvme_controller& controller, uint32_t nsid) : controller(controller), nsid(nsid) { }

    ~nvme_namespace() {
        lock_guard lock(controller.namespaces_lock);
        remove_entry_list(le);
    }

    void release() override {
        if (--refcount == 0)
            delete this;
    }

    nt_status flush() override;

    nt_status dio_read(uint64_t page_num, uint64_t num_pages,
                       pfn_iterator& it, uint64_t& read_pages) override;
    nt_status dio_write(uint64_t page_num, uint64_t num_pages,
                        pfn_iterator& it, uint64_t& written_pages,
                        bool fua) override;

    list_entry le;
    nvme_controller& controller;
    uint32_t nsid;
    rwsem flush_sem;
};

static bool init_nvme(const pci_device& dev);

static pci_class_recognizer mod_pci_class = {
    {}, pci_class::mass_storage, (uint8_t)pci_mass_storage::nonvolatile_memory, false, init_nvme
};

static module_init module_init_nvme([]() {
    insert_tail_list(pci_classes, mod_pci_class.le);
});

static bool nvme_msix_isr_func(void* ctx) {
    return ((nvme_queue*)ctx)->isr();
}

static bool nvme_isr_func(void* ctx) {
    return ((nvme_controller*)ctx)->isr();
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

    bool msi = msi_supported && dev.msix;

    if (msi) {
        // enable MSI-X (FIXME - why does this need to be done here, when setup_msi does it too?)
        dev.msix->message_control |= 0x8000;
    }

    reset_controller(admin_sq_phys, admin_cq_phys);

    admin_queue.sq = phys_address(admin_sq_phys);
    admin_queue.cq = phys_address(admin_cq_phys);

    if (msi) {
        // set up admin queue interrupt
        auto int_num = setup_msi(dev, true, nvme_msix_isr_func, &admin_queue);
        if (int_num == 0)
            return;

#ifdef __aarch64__
        admin_queue.irq_num = int_num;
#endif
        admin_queue.isr_registered = true;
    } else {
        if (!add_irq_handler(*dev.irq, true, nvme_isr_func, this)) {
            print("add_irq_handler failed\n");
            return;
        }
    }

    if (!identify_controller(admin_queue))
        return;

    if (!create_io_queues(dev, admin_queue))
        return;

    if (!identify_namespaces(admin_queue))
        return;

    initialized = true;
}

nvme_controller::~nvme_controller() {
    if (controller_info)
        free_folio(phys_memory_pfn(controller_info) << PAGE_SHIFT);

    // FIXME - if not MSI, call remove_irq_handler
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

        allocated_slots &= ~((uint32_t)1 << slot);
    }

    release_semaphore(static_cast<semaphore&>(sem.get()));
}

void nvme_queue::submit(nvme_request& req) {
    req.event = create_event(true);

    // FIXME - if get_slot would block, we should be queueing this on a waitlist instead

    auto slot = get_slot();

    req.sqe.cid = slot;

    size_t sqe_size;

    if (queue_id == 0)
        sqe_size = sizeof(nvme_command);
    else
        sqe_size = 1 << controller.controller_info->minsqes;

    {
        lock_guard lock(sq_lock);

        memcpy((uint8_t*)sq + (sq_tail * sqe_size), &req.sqe, sizeof(req.sqe));

        if (sizeof(req.sqe) < sqe_size) {
            memset((uint8_t*)sq + (sq_tail * sqe_size) + sizeof(req.sqe),
                   0, sqe_size - sizeof(req.sqe));
        }

        reqs[slot] = &req;

        flush_cache(span((uint8_t*)sq + (sq_tail * sqe_size), sqe_size), false);

        sq_tail++;

        if (sq_tail == QUEUE_SIZE)
            sq_tail = 0;

        auto sq_doorbell = (volatile uint32_t*)((uint8_t*)controller.regs + 0x1000 + (2 * controller.doorbell_stride * queue_id));

        *sq_doorbell = sq_tail;
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

    flush_cache(span((uint8_t*)phys_address(p), sizeof(nvme_identify_controller_data)),
                false);

    queue.submit(req);

    wait_for_object(req.event.get(), false);

    if (req.cqe.status != 0) {
        print("NVMe identify returned status {:x}\n", req.cqe.status);
        free_folio(p);
        return false;
    }

    flush_cache(span((uint8_t*)phys_address(p), sizeof(nvme_identify_controller_data)),
                true);

    controller_info = (nvme_identify_controller_data*)phys_address(p);

    if (controller_info->cntrltype != 0 && controller_info->cntrltype != 1) {
        print("nvme_controller::identify_controller: controller type was {}, expected 0 or 1\n",
              controller_info->cntrltype);
        return false;
    }

    uint8_t mpsmin = (*(&regs->cap) & 0xf000000000000) >> 48;

    if (controller_info->mdts == 0)
        maximum_transfer_size = 0;
    else
        maximum_transfer_size = 1 << (controller_info->mdts + mpsmin + 12);

    volatile uint32_t* cc = &regs->cc;

    *cc = (*cc & ~CC_IOCQES_MASK) | (uint32_t)(controller_info->mincqes << CC_IOCQES_SHIFT);
    *cc = (*cc & ~CC_IOSQES_MASK) | (uint32_t)(controller_info->minsqes << CC_IOSQES_SHIFT);

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

        auto fidxl = (size_t)(nsinfo.flbas & 0xf);

        auto num_lba_formats = min(nsinfo.nlbaf, (uint8_t)nsinfo.lbaf.size());

        if (num_lba_formats > 16)
            fidxl |= (nsinfo.flbas & 0x30) >> 1;

        if (fidxl > num_lba_formats) {
            print("skipping NVMe namespace {}: FIDXL was {}, expected 0 to {}\n",
                  req2.req.sqe.nsid, fidxl, num_lba_formats - 1);
            free_folio(req2.p);
            delete &req2;
            continue;
        }

        if (!(nsinfo.lbaf[fidxl] & 0xf0000)) {
            print("skipping NVMe namespace {}: LBADS was 0\n", req2.req.sqe.nsid);
            free_folio(req2.p);
            delete &req2;
            continue;
        }

        auto block_size = (unsigned int)1 << ((nsinfo.lbaf[fidxl] & 0xf0000) >> 16);

        // FIXME - support block_size > PAGE_SIZE?

        if (block_size > PAGE_SIZE) {
            print("skipping NVMe namespace {}: block size {} more than page size {}\n",
                  req2.req.sqe.nsid, block_size, PAGE_SIZE);
            free_folio(req2.p);
            delete &req2;
            continue;
        }

        if (PAGE_SIZE % block_size) {
            print("skipping NVMe namespace {}: page size {} not a multiple of block size {}\n",
                  req2.req.sqe.nsid, PAGE_SIZE, block_size);
            free_folio(req2.p);
            delete &req2;
            continue;
        }

        string_view model_number(controller_info->mn, sizeof(controller_info->mn));

        while (!model_number.empty() && (model_number.back() == 0 || model_number.back() == ' ')) {
            model_number = string_view(model_number.data(), model_number.size() - 1);
        }

        print("found /dev/nvme{}n{}: {}, block size {}, capacity {}\n", controller_num,
              req2.req.sqe.nsid, model_number, byte_size(block_size), byte_size(nsinfo.nsze * block_size));

        // FIXME - readonly if nsattr & 1

        auto pdev = new nvme_namespace(*this, req2.req.sqe.nsid);
        if (!pdev) {
            print("malloc failed\n");
            free_folio(req2.p);
            delete &req2;
            continue;
        }

        auto& dev = *pdev;

        {
            lock_guard lock(namespaces_lock);
            insert_tail_list(namespaces, dev.le);
        }

        {
            char s[50];
            auto sp = span(s, sizeof(s));

            sp = format(sp, "nvme{}n{}", controller_num, req2.req.sqe.nsid);

            auto sv = string_view(s, (size_t)(sp.data() - s));

            dev.volume_name_len = sv.size();
            dev.volume_name = (char*)malloc(sv.size());
            if (!dev.volume_name) {
                print("malloc failed\n");
                dev.release();
                free_folio(req2.p);
                delete &req2;
                continue;
            }

            memcpy(dev.volume_name, sv.data(), sv.size());

            auto status = name_object(dev, sv);
            if (!success(status)) {
                print("name_object returned {}\n", status);
                dev.release();
                free_folio(req2.p);
                delete &req2;
                continue;
            }

            dev.sector_size = block_size;
            dev.size = nsinfo.nsze * block_size;
        }

        dev.probe();
        dev.release();

        free_folio(req2.p);
        delete &req2;
    }

    return true;
}

static bool init_nvme(const pci_device& dev) {
    bool msi;

    if (!dev.irq.has_value()) {
        if (!dev.msix) {
            print("init_nvme: MSI-X not supported, and no IRQ number\n");
            return false;
        } else if (!msi_supported) {
            print("init_nvme: no IRQ number, and MSI-X disabled or not supported\n");
            return false;
        }
    }

    msi = msi_supported && dev.msix;

    if (msi) {
        // FIXME - support MSI too?
        if (dev.msix->bir >= dev.bar.size()) {
            print("invalid value {} for NVMe MSI-X BIR\n", dev.msix->bir);
            return false;
        }

        if ((size_t)(dev.msix->table_offset << 3) >= dev.bar[dev.msix->bir].size()) {
            print("NVMe MSI-X table offset {:x} overflowed BAR {}\n",
                  dev.msix->table_offset << 3, dev.msix->bir);
            return false;
        }
    }

    auto n = new nvme_controller(dev);
    if (!n) {
        print("malloc failed\n");
        return false;
    }

    if (!n->initialized) {
        delete n;
        return false;
    }

    {
        lock_guard lock(nvme_lock);
        insert_tail_list(nvme_controllers, n->le);
    }

    return true;
}

bool nvme_queue::isr() {
    size_t cqe_size;

    if (queue_id == 0)
        cqe_size = sizeof(nvme_completion);
    else
        cqe_size = 1 << controller.controller_info->mincqes;

    auto old_cq_head = cq_head;

    {
        lock_guard lock(sq_lock);

        while (cq_head != sq_tail) {
            auto& comp = *(nvme_completion*)((uint8_t*)cq + (cq_head * cqe_size));

            if (comp.p != exp_cq_p)
                break;

            if (comp.cid >= QUEUE_SIZE)
                bsod_format("NVMe completion for unknown cid {:x}\n", (uint16_t)comp.cid);

            memcpy(&reqs[comp.cid]->cqe, &comp, sizeof(reqs[comp.cid]->cqe));

            cq_head++;

            if (cq_head == QUEUE_SIZE) {
                cq_head = 0;
                exp_cq_p = exp_cq_p == 1 ? 0 : 1;
            }
        }

        if (cq_head == old_cq_head)
            return false;

        while (old_cq_head != cq_head) {
            auto& comp = *(nvme_completion*)((uint8_t*)cq + (old_cq_head * cqe_size));

            auto& req = *reqs[comp.cid];
            reqs[comp.cid] = nullptr;

            release_slot((uint8_t)comp.cid);

            set_event(static_cast<::event&>(req.event.get()));

            old_cq_head++;

            if (old_cq_head == QUEUE_SIZE)
                old_cq_head = 0;
        }
    }

    barrier();

    auto cq_doorbell = (volatile uint32_t*)((uint8_t*)controller.regs + 0x1000 + (((2 * queue_id) + 1) * controller.doorbell_stride));

    *cq_doorbell = cq_head;

    return true;
}

bool nvme_controller::isr() {
    if (admin_queue.isr())
        return true;

    return io_queue.isr();
}

bool nvme_controller::create_io_queues(const pci_device& dev, nvme_queue& queue) {
    // FIXME - should be creating one pair of queues for each CPU?

    // create completion queue

    auto cq = allocate_folio(bytes_to_folio_order((1 << controller_info->mincqes) * QUEUE_SIZE));
    if (!cq) {
        print("allocate_folio failed\n");
        return false;
    }

    memset(phys_address(cq), 0, (1 << controller_info->mincqes) * QUEUE_SIZE);

    {
        nvme_request req;

        memset(&req.sqe, 0, sizeof(req.sqe));

        auto& sqe = *reinterpret_cast<nvme_create_cq_command*>(&req.sqe);

        sqe.opc = NVME_ADMIN_OPCODE_CREATE_CQ;
        sqe.fuse = 0;
        sqe.psdt = 0; // use PRPs
        sqe.nsid = 0;
        sqe.dptr.prp1 = cq;
        sqe.qid = 1;
        sqe.qsize = QUEUE_SIZE - 1;
        sqe.pc = 1;
        sqe.ien = 1;
        sqe.iv = (dev.msix && msi_supported) ? 1 : 0;

        queue.submit(req);

        wait_for_object(req.event.get(), false);

        if (req.cqe.status != 0) {
            print("NVMe create completion queue returned status {:x}\n", req.cqe.status);
            free_folio(cq);
            return false;
        }
    }

    // create submission queue

    auto sq = allocate_folio(bytes_to_folio_order((1 << controller_info->minsqes) * QUEUE_SIZE));
    if (!sq) {
        print("allocate_folio failed\n");
        // FIXME - delete CQ and free its buffer
        return false;
    }

    {
        nvme_request req;

        memset(&req.sqe, 0, sizeof(req.sqe));

        auto& sqe = *reinterpret_cast<nvme_create_sq_command*>(&req.sqe);

        sqe.opc = NVME_ADMIN_OPCODE_CREATE_SQ;
        sqe.fuse = 0;
        sqe.psdt = 0; // use PRPs
        sqe.nsid = 0;
        sqe.dptr.prp1 = sq;
        sqe.qid = 1;
        sqe.qsize = QUEUE_SIZE - 1;
        sqe.pc = 1;
        sqe.qprio = 0;
        sqe.cqid = 1;
        sqe.nvmsetid = 0;

        queue.submit(req);

        wait_for_object(req.event.get(), false);

        if (req.cqe.status != 0) {
            print("NVMe create submission queue returned status {:x}\n", req.cqe.status);
            // FIXME - delete CQ and free its buffer
            free_folio(sq);
            return false;
        }
    }

    io_queue.cq = phys_address(cq);
    io_queue.sq = phys_address(sq);

    // FIXME - free interrupt when we go away
    auto int_num = add_msix_entry(dev, 1, nvme_msix_isr_func, &io_queue);
    if (int_num == 0) {
        // FIXME - delete CQ and SQ
        return false;
    }

#ifdef __aarch64__
    io_queue.irq_num = int_num;
#endif
    io_queue.isr_registered = true;

    return true;
}

nt_status nvme_namespace::dio_read(uint64_t page_num, uint64_t num_pages,
                                   pfn_iterator& it, uint64_t& read_pages) {
    static constexpr size_t PRP_LIST_ENTRIES = PAGE_SIZE / sizeof(uint64_t);

    if (num_pages << PAGE_SHIFT > controller.maximum_transfer_size)
        num_pages = controller.maximum_transfer_size >> PAGE_SHIFT;

    // FIXME - support linked PRP lists (last entry should be pointer to next list)
    // not an issue on Qemu, which only supports transfers up to 512KB - is this
    // larger on real hardware?
    if (num_pages >= PRP_LIST_ENTRIES)
        num_pages = PRP_LIST_ENTRIES;

    uint64_t num_blocks = (num_pages << PAGE_SHIFT) / sector_size;

    // put read request into controller's IO queue

    auto& queue = controller.io_queue;

    nvme_request req;

    memset(&req.sqe, 0, sizeof(req.sqe));

    auto& sqe = *reinterpret_cast<nvme_read_command*>(&req.sqe);

    uint64_t prp1, prp2;

    prp1 = *it;

    if (num_pages <= 2) {
        if (num_pages == 2) {
            it++;
            prp2 = *it;
        } else
            prp2 = 0;
    } else {
        prp2 = allocate_folio(0);
        if (!prp2) {
            print("allocate_folio failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        auto ptr = (uint64_t*)phys_address(prp2);

        for (uint64_t i = 1; i < num_pages; i++) {
            it++;
            *ptr = *it;
            ptr++;
        }
    }

    sqe.opc = NVME_IO_OPCODE_READ;
    sqe.fuse = 0;
    sqe.psdt = 0; // use PRPs
    sqe.nsid = nsid;
    sqe.elbtu = 0;
    sqe.mptr = 0;
    sqe.dptr.prp1 = prp1;
    sqe.dptr.prp2 = prp2;
    sqe.slba = (page_num << PAGE_SHIFT) / sector_size;
    sqe.nlb = (uint16_t)(num_blocks - 1);
    sqe.cetype = 0;
    sqe.stc = 0;
    sqe.prinfo = 0;
    sqe.fua = 0;
    sqe.lr = 0;
    sqe.dsm = 0;
    sqe.elbtl = 0;
    sqe.elbat = 0;
    sqe.elbatm = 0;

    queue.submit(req);

    wait_for_object(req.event.get(), false);

    if (num_pages > 2)
        free_folio(prp2);

    if (req.cqe.status != 0) {
        print("NVMe read returned status {:x}\n", req.cqe.status);
        // FIXME - translate into nt_status properly
        return nt_status::INTERNAL_ERROR;
    }

    read_pages = num_pages;

    return nt_status::SUCCESS;
}

nt_status nvme_namespace::dio_write(uint64_t page_num, uint64_t num_pages,
                                    pfn_iterator& it, uint64_t& written_pages,
                                    bool fua) {
    static constexpr size_t PRP_LIST_ENTRIES = PAGE_SIZE / sizeof(uint64_t);

    if (num_pages << PAGE_SHIFT > controller.maximum_transfer_size)
        num_pages = controller.maximum_transfer_size >> PAGE_SHIFT;

    // FIXME - support linked PRP lists (last entry should be pointer to next list)
    // not an issue on Qemu, which only supports transfers up to 512KB - is this
    // larger on real hardware?
    if (num_pages >= PRP_LIST_ENTRIES)
        num_pages = PRP_LIST_ENTRIES;

    uint64_t num_blocks = (num_pages << PAGE_SHIFT) / sector_size;

    // put write request into controller's IO queue

    auto& queue = controller.io_queue;

    nvme_request req;

    memset(&req.sqe, 0, sizeof(req.sqe));

    auto& sqe = *reinterpret_cast<nvme_write_command*>(&req.sqe);

    uint64_t prp1, prp2;

    prp1 = *it;

    if (num_pages <= 2) {
        if (num_pages == 2) {
            it++;
            prp2 = *it;
        } else
            prp2 = 0;
    } else {
        prp2 = allocate_folio(0);
        if (!prp2) {
            print("allocate_folio failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        auto ptr = (uint64_t*)phys_address(prp2);

        for (uint64_t i = 1; i < num_pages; i++) {
            it++;
            *ptr = *it;
            ptr++;
        }
    }

    sqe.opc = NVME_IO_OPCODE_WRITE;
    sqe.fuse = 0;
    sqe.psdt = 0; // use PRPs
    sqe.nsid = nsid;
    sqe.lbtu = 0;
    sqe.mptr = 0;
    sqe.dptr.prp1 = prp1;
    sqe.dptr.prp2 = prp2;
    sqe.slba = (page_num << PAGE_SHIFT) / sector_size;
    sqe.nlb = (uint16_t)(num_blocks - 1);
    sqe.cetype = 0;
    sqe.stc = 0;
    sqe.prinfo = 0;
    sqe.fua = fua ? 1 : 0;
    sqe.lr = 0;
    sqe.dsm = 0;
    sqe.lbtl = 0;
    sqe.lbat = 0;
    sqe.lbatm = 0;

    shared_lock lock(flush_sem);

    queue.submit(req);

    wait_for_object(req.event.get(), false);

    if (num_pages > 2)
        free_folio(prp2);

    if (req.cqe.status != 0) {
        print("NVMe write returned status {:x}\n", req.cqe.status);
        // FIXME - properly translate into nt_status
        return nt_status::INTERNAL_ERROR;
    }

    written_pages = num_pages;

    return nt_status::SUCCESS;
}

nt_status nvme_namespace::flush() {
    auto& queue = controller.io_queue;

    nvme_request req;

    memset(&req.sqe, 0, sizeof(req.sqe));

    auto& sqe = req.sqe;

    sqe.opc = NVME_IO_OPCODE_FLUSH;
    sqe.nsid = nsid;

    lock_guard lock(flush_sem);

    queue.submit(req);

    wait_for_object(req.event.get(), false);

    if (req.cqe.status != 0) {
        print("NVMe flush returned status {:x}\n", req.cqe.status);
        // FIXME - translate into nt_status properly
        return nt_status::INTERNAL_ERROR;
    }

    return nt_status::SUCCESS;
}
