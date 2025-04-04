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
import format;
import config;
import mem;
import interrupts;
import event;
import semaphore;
import volume;
import bsod;

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

static void nvme_isr_func(void* ctx);

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

        if (isr_registered)
            remove_isr(nvme_isr_func, this);
    }

    nvme_controller& controller;
    unsigned int queue_id;
    uint8_t get_slot();
    void release_slot(uint8_t slot);
    void isr();
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

    size_t read_into_cache(uint64_t page_num, uint64_t num_pages) override;
    size_t flush_cache(uint64_t page_num, uint64_t num_pages) override;

    list_entry le;
    nvme_controller& controller;
    uint32_t nsid;
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

    auto int_num = add_isr(nvme_isr_func, &admin_queue);
    if (int_num == 0) {
        print("add_isr failed\n");
        return;
    }

    admin_queue.isr_registered = true;

    vec.message_data = int_num;
    vec.message_address_high = 0;
    vec.message_address_low = apic_msi_address(0, false, false); // FIXME - spread out amongst processors
    vec.vector_control &= ~(uint32_t)1; // unmask

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

static constexpr uint8_t bytes_to_folio_order(size_t s) {
    uint32_t p = bytes_to_pages(s);

    if (p == 0)
        return 0;

    return (uint8_t)(32 - countl_zero(p - 1));
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

        barrier();

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

    queue.submit(req);

    wait_for_object(req.event.get(), false);

    if (req.cqe.status != 0) {
        print("NVMe identify returned status {:x}\n", req.cqe.status);
        free_folio(p);
        return false;
    }

    controller_info = (nvme_identify_controller_data*)phys_address(p);

    if (controller_info->cntrltype != 1) {
        print("not an I/O controller\n");
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

        if (fidxl >= num_lba_formats) {
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

            if (!name_object(dev, sv)) {
                print("name_object failed\n");
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
    size_t cqe_size;

    if (queue_id == 0)
        cqe_size = sizeof(nvme_completion);
    else
        cqe_size = 1 << controller.controller_info->mincqes;

    auto old_cq_head = cq_head;

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
        return;

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

    barrier();

    auto cq_doorbell = (volatile uint32_t*)((uint8_t*)controller.regs + 0x1000 + (((2 * queue_id) + 1) * controller.doorbell_stride));

    *cq_doorbell = cq_head;
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
        sqe.iv = 1;

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

    // set up interrupt

    auto vecs = (volatile msix_entry*)(dev.bar[dev.msix->bir].data() + (dev.msix->table_offset << 3));

    auto int_num = add_isr(nvme_isr_func, &io_queue);
    if (int_num == 0) {
        print("add_isr failed\n");
        // FIXME - delete CQ and SQ
        return false;
    }

    io_queue.isr_registered = true;

    vecs[1].message_data = int_num;
    vecs[1].message_address_high = 0;
    vecs[1].message_address_low = apic_msi_address(0, false, false); // FIXME - spread out amongst processors
    vecs[1].vector_control &= ~(uint32_t)1; // unmask

    return true;
}

size_t nvme_namespace::read_into_cache(uint64_t page_num, uint64_t num_pages) {
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

    if (num_pages <= 2) {
        lock_guard lg(cache.cache_lock);

        auto* p = cache.find_page_cache_entry(page_num);

        prp1 = p->phys_address;

        if (num_pages == 2) {
            p = cache.next_page_cache_entry(*p);
            prp2 = p->phys_address;
        } else
            prp2 = 0;
    } else {
        prp2 = allocate_folio(0);
        if (!prp2) {
            print("allocate_folio failed\n");
            return 0;
        }

        auto ptr = (uint64_t*)phys_address(prp2);

        lock_guard lg(cache.cache_lock);

        auto* p = cache.find_page_cache_entry(page_num);

        prp1 = p->phys_address;

        for (uint64_t i = 1; i < num_pages; i++) {
            p = cache.next_page_cache_entry(*p);
            *ptr = p->phys_address;
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
        // FIXME - translate into nt_status and return somehow
        return 0;
    }

    return num_pages;
}

size_t nvme_namespace::flush_cache(uint64_t page_num, uint64_t num_pages) {
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

    if (num_pages <= 2) {
        lock_guard lg(cache.cache_lock);

        auto* p = cache.find_page_cache_entry(page_num);

        prp1 = p->phys_address;

        if (num_pages == 2) {
            p = cache.next_page_cache_entry(*p);
            prp2 = p->phys_address;
        } else
            prp2 = 0;
    } else {
        prp2 = allocate_folio(0);
        if (!prp2) {
            print("allocate_folio failed\n");
            return 0;
        }

        auto ptr = (uint64_t*)phys_address(prp2);

        lock_guard lg(cache.cache_lock);

        auto* p = cache.find_page_cache_entry(page_num);

        prp1 = p->phys_address;

        for (uint64_t i = 1; i < num_pages; i++) {
            p = cache.next_page_cache_entry(*p);
            *ptr = p->phys_address;
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
    sqe.fua = 0;
    sqe.lr = 0;
    sqe.dsm = 0;
    sqe.lbtl = 0;
    sqe.lbat = 0;
    sqe.lbatm = 0;

    queue.submit(req);

    wait_for_object(req.event.get(), false);

    if (num_pages > 2)
        free_folio(prp2);

    if (req.cqe.status != 0) {
        print("NVMe write returned status {:x}\n", req.cqe.status);
        // FIXME - translate into nt_status and return somehow
        return 0;
    }

    return num_pages;
}
