module;

#include <optional>
#include <mutex>
#include <algorithm>
#include <span>
#include "acpica.h"
#include "virtio.h"
#include <variant>

export module virtio;

import nt;
import print;
import mem;
import config;
import interrupts;
import semaphore;
import event;
import pci;
import acpi;
import module_init;
import apic;
import object_ref;
import scheduler;
import list;
import spinlock;

using namespace std;

// FIXME - allow dynamic queue size (maximum supported by device?)
const size_t QUEUE_SIZE = 32;

export const uint8_t VIRTQ_DESC_F_NEXT = 1;
export const uint8_t VIRTQ_DESC_F_WRITE = 2;
export const uint8_t VIRTQ_DESC_F_INDIRECT = 4;

export const uint64_t VIRTIO_RING_F_INDIRECT_DESC = 1 << 28;
export const uint64_t VIRTIO_RING_F_EVENT_IDX = 1 << 29;
export const uint64_t VIRTIO_F_VERSION_1 = (uint64_t)1 << 32;
export const uint64_t VIRTIO_F_ACCESS_PLATFORM = (uint64_t)1 << 33;
export const uint64_t VIRTIO_F_RING_PACKED = (uint64_t)1 << 34;
export const uint64_t VIRTIO_F_IN_ORDER = (uint64_t)1 << 35;
export const uint64_t VIRTIO_F_ORDER_PLATFORM = (uint64_t)1 << 36;
export const uint64_t VIRTIO_F_SR_IOV = (uint64_t)1 << 37;
export const uint64_t VIRTIO_F_NOTIFICATION_DATA = (uint64_t)1 << 38;
export const uint64_t VIRTIO_F_RING_RESET = (uint64_t)1 << 40;

static const uint32_t VIRTIO_MMIO_MAGIC_VALUE = 0x74726976; // "virt"

export struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute((packed));

static_assert(sizeof(virtq_desc) == 16);

template<unsigned N>
struct virtq {
    virtq_desc buffers[N];
    virtq_avail<N> available;
    uint8_t padding[PAGE_SIZE - (N * sizeof(virtq_desc)) - sizeof(virtq_avail<N>)];
    virtq_used<N> used;
} __attribute((packed));

export class virt_queue {
public:
    virt_queue();
    ~virt_queue();
    nt_status read(span<uint8_t> msg, uint64_t& read_bytes);
    nt_status write(span<const uint8_t> msg);
    nt_status indirect(span<uint8_t> s, uint64_t& read_bytes);
    void isr();

    virtq<QUEUE_SIZE>* queue;
    uint64_t queue_pa;
    list_entry le;
    uint16_t num;
    volatile uint32_t* notification_addr;

private:
    uint8_t get_slot();
    void release_slot(uint8_t slot);

    uint32_t allocated_slots = 0;
    uint16_t old_used_idx = 0;
    spinlock slot_lock, submit_lock;
    object_ref sem{create_semaphore(QUEUE_SIZE)};
    object_ref events[QUEUE_SIZE];
    uint32_t lengths[QUEUE_SIZE];
};

export class virtio_pci_device {
public:
    virtio_pci_device(pci_device& dev, uint64_t supported_features,
                      uint16_t num_queues);
    ~virtio_pci_device();
    void finalize();

    virtio_pci_device(const virtio_pci_device&) = delete;

    pci_device& dev;
    bool initialized = false;
    list_entry virtqueues = { &virtqueues, &virtqueues };
    virtio_pci_cap* device_cfg = nullptr;
    uint64_t negotiated_features = 0;

private:
    bool add_virtqueue(uint16_t num);

    volatile virtio_pci_common_cfg* common;
    virtio_pci_notify_cap* notify_cfg = nullptr;
};

export class virtio_mmio_device {
public:
    virtio_mmio_device(volatile virtio_mmio_regs_v1& regs,
                       uint64_t supported_features, uint16_t num_queues,
                       void* config);
    virtio_mmio_device(volatile virtio_mmio_regs_v2& regs,
                       uint64_t supported_features, uint16_t num_queues,
                       void* config);
    ~virtio_mmio_device();
    void finalize();
    bool isr();

    virtio_mmio_device(const virtio_mmio_device&) = delete;

    bool initialized = false;
    list_entry virtqueues = { &virtqueues, &virtqueues };
    uint64_t negotiated_features = 0;
    void* config;

private:
    bool add_virtqueue(uint16_t num);

    variant<volatile virtio_mmio_regs_v1*, volatile virtio_mmio_regs_v2*> var_regs;
};

export struct virtio_mmio_recognizer {
    list_entry le;
    uint32_t device_id;
    uint64_t supported_features;
    uint16_t num_queues;
    bool (*func)(virtio_mmio_device&);
};

export list_entry virtio_mmio_types = { &virtio_mmio_types, &virtio_mmio_types };

static void found_virtio_mmio(const acpi_device& d);

static acpi_recognizer mod_pnp_hids[] = {
    { {}, "LNRO0005", found_virtio_mmio, nullptr }
};

static void init_acpi_hids() {
    for (unsigned int i = 0; i < sizeof(mod_pnp_hids) / sizeof(*mod_pnp_hids); i++) {
        insert_tail_list(pnp_hids, mod_pnp_hids[i].le);
    }
}

static module_init module_init_virtio_mmio(init_acpi_hids);

static bool check_cap(pci_device& dev, const virtio_pci_cap& cap) {
    if (cap.cap_len < sizeof(virtio_pci_cap) + sizeof(pci_capability)) {
        print("check_cap: capability length was {}, expected at least {}\n",
              cap.cap_len, sizeof(virtio_pci_cap) + sizeof(pci_capability));
        return false;
    }

    if (cap.bar >= dev.bar.size()) {
        print("check_cap: cfg BAR was {}, expected 0 to {}\n",
              cap.bar, dev.bar.size());
        return false;
    }

    if (cap.offset >= dev.bar[cap.bar].size()) {
        print("check_cap: cfg started at {:x}, beyond end of BAR {} ({:x})\n",
              cap.offset, cap.bar, dev.bar[cap.bar].size() - 1);
        return false;
    }

    if (cap.offset + cap.length > dev.bar[cap.bar].size()) {
        print("check_cap: cfg ended at {:x}, beyond end of BAR {} ({:x})\n",
              cap.offset + cap.length - 1, cap.bar,
              dev.bar[cap.bar].size() - 1);
        return false;
    }

    return true;
}

static void write_volatile64(volatile uint64_t* addr, uint64_t val) {
    *(volatile uint32_t*)addr = val & 0xffffffff;
    *((volatile uint32_t*)addr + 1) = (uint32_t)(val >> 32);
}

static void virtio_pci_isr_func(void* ctx) {
    auto& vq = *(virt_queue*)ctx;

    vq.isr();
}

bool virtio_pci_device::add_virtqueue(uint16_t num) {
    common->queue_select = num;

    if (common->queue_size < QUEUE_SIZE) {
        print("virtio_pci_device::add_virtqueue: size for queue {} was {}, wanted at least {}\n",
              common->queue_select, common->queue_size, QUEUE_SIZE);
        return false;
    }

    common->queue_size = QUEUE_SIZE;

    static const size_t NUM_PAGES = bytes_to_pages(sizeof(virtq<QUEUE_SIZE>));

    auto pa = allocate_folio(bytes_to_folio_order(sizeof(virtq<QUEUE_SIZE>)));
    if (!pa) {
        print("allocate folio failed\n");
        return false;
    }

    auto va = allocate_va(NUM_PAGES);
    if (!va) {
        print("allocate_va failed\n");
        free_folio(pa);
        return false;
    }

    for (unsigned int i = 0; i < NUM_PAGES; i++) {
        if (!map_page(pa + (i << PAGE_SHIFT), va + (i << PAGE_SHIFT), mapping_type::no_cache)) {
            print("map_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                unmap_page(va + (j << PAGE_SHIFT));
            }

            free_va(va, NUM_PAGES);
            free_folio(pa);

            return false;
        }
    }

    auto vq = new virt_queue;
    if (!vq) {
        print("new failed\n");

        for (unsigned int i = 0; i < NUM_PAGES; i++) {
            unmap_page(va + (i << PAGE_SHIFT));
        }

        free_va(va, NUM_PAGES);
        free_folio(pa);
        return false;
    }

    auto& queue = *(virtq<QUEUE_SIZE>*)va;

    memset(&queue, 0, sizeof(queue));

    vq->num = num;
    vq->queue = &queue;
    vq->queue_pa = pa;

    auto int_num = add_isr(virtio_pci_isr_func, vq);
    if (int_num == 0) {
        print("add_isr failed\n");
        delete vq;
        return false;
    }

    write_volatile64(&common->queue_desc, pa);
    write_volatile64(&common->queue_driver, pa + ((uintptr_t)&queue.available - (uintptr_t)&queue));
    write_volatile64(&common->queue_device, pa + ((uintptr_t)&queue.used - (uintptr_t)&queue));

    uint16_t msix_vector = num;

    common->queue_notify_off = num;
    common->queue_msix_vector = msix_vector;

    if (common->queue_msix_vector != msix_vector) {
        print("virtio_pci_device::add_virtqueue: setting MSI-X vector failed\n");
        delete vq;
        return false;
    }

    auto vec = (volatile msix_entry*)(dev.bar[dev.msix->bir].data() + (dev.msix->table_offset << 3));

    vec[msix_vector].message_data = int_num;
    vec[msix_vector].message_address_high = 0;
    vec[msix_vector].message_address_low = apic_msi_address(0, false, false); // FIXME - spread out amongst processors
    vec[msix_vector].vector_control &= ~(uint32_t)1; // unmask

    common->queue_enable = 1;

    vq->notification_addr = (volatile uint32_t*)(dev.bar[notify_cfg->bar].data() +
                                                 notify_cfg->offset +
                                                 (num * notify_cfg->notify_off_multiplier));

    insert_tail_list(virtqueues, vq->le);

    return true;
}

uint8_t virt_queue::get_slot() {
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

void virt_queue::release_slot(uint8_t slot) {
    {
        lock_guard l(slot_lock);

        allocated_slots &= ~((uint32_t)1 << slot);
    }

    release_semaphore(static_cast<semaphore&>(sem.get()));
}

nt_status virt_queue::read(span<uint8_t> s, uint64_t& read_bytes) {
    // FIXME - do without copying

    if (s.empty()) {
        read_bytes = 0;
        return nt_status::SUCCESS;
    }

    auto buf_size = min(s.size(), PAGE_SIZE);
    auto buf = malloc(buf_size);
    if (!buf) {
        print("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto slot = get_slot();

    queue->buffers[slot].addr = va_to_pa_phys(buf);
    queue->buffers[slot].len = (uint32_t)buf_size;
    queue->buffers[slot].flags = VIRTQ_DESC_F_WRITE;

    {
        lock_guard lock(submit_lock);

        queue->available.ring[queue->available.idx % QUEUE_SIZE] = slot;

        barrier();
        __asm __volatile("sfence" ::: "memory");

        queue->available.idx++;
    }

    *notification_addr = num;

    // FIXME - make interruptible
    wait_for_object(events[slot].get(), false);

    read_bytes = lengths[slot];

    release_slot(slot);

    memcpy(s.data(), buf, read_bytes);

    free(buf);

    return nt_status::SUCCESS;
}

nt_status virt_queue::indirect(span<uint8_t> s, uint64_t& read_bytes) {
    if (s.empty()) {
        read_bytes = 0;
        return nt_status::SUCCESS;
    }

    auto slot = get_slot();

    // s needs to be in phys memory range

    queue->buffers[slot].addr = va_to_pa_phys(s.data());
    queue->buffers[slot].len = (uint32_t)s.size();
    queue->buffers[slot].flags = VIRTQ_DESC_F_INDIRECT;

    {
        lock_guard lock(submit_lock);

        queue->available.ring[queue->available.idx % QUEUE_SIZE] = slot;

        barrier();
        __asm __volatile("sfence" ::: "memory");

        queue->available.idx++;
    }

    *notification_addr = num;

    // FIXME - make interruptible
    wait_for_object(events[slot].get(), false);

    read_bytes = lengths[slot];

    release_slot(slot);

    return nt_status::SUCCESS;
}

nt_status virt_queue::write(span<const uint8_t> msg) {
    // FIXME - do without copying

    auto buf = malloc(msg.size());
    if (!buf) {
        print("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    memcpy(buf, msg.data(), msg.size());

    auto slot = get_slot();

    queue->buffers[slot].addr = va_to_pa_phys(buf);
    queue->buffers[slot].len = (uint32_t)msg.size();
    queue->buffers[slot].flags = 0;

    {
        lock_guard lock(submit_lock);

        queue->available.ring[queue->available.idx % QUEUE_SIZE] = slot;

        barrier();
        __asm __volatile("sfence" ::: "memory");

        queue->available.idx++;
    }

    *notification_addr = num;

    // FIXME - make interruptible
    wait_for_object(events[slot].get(), false);

    release_slot(slot);

    free(buf);

    return nt_status::SUCCESS;
}

virtio_pci_device::virtio_pci_device(pci_device& dev, uint64_t supported_features,
                                     uint16_t num_queues) : dev(dev) {
    virtio_pci_cap* common_cfg = nullptr;

    if (!dev.msix) {
        print("MSI-X not supported for virtio device\n");
        return;
    }

    for (auto le = dev.vendor_specific_caps.next; le != &dev.vendor_specific_caps; le = le->next) {
        auto& vs = containing_record<vendor_specific_cap>(le);
        auto& cap = *(virtio_pci_cap*)vs.ptr;

        if (cap.cfg_type == VIRTIO_PCI_CAP::COMMON_CFG && !common_cfg)
            common_cfg = &cap;
        else if (cap.cfg_type == VIRTIO_PCI_CAP::DEVICE_CFG && !device_cfg)
            device_cfg = &cap;
        else if (cap.cfg_type == VIRTIO_PCI_CAP::NOTIFY_CFG && !notify_cfg)
            notify_cfg = reinterpret_cast<virtio_pci_notify_cap*>(&cap);
    }

    if (!common_cfg) {
        print("virtio_pci_device: no COMMON_CFG capability found\n");
        return;
    }

    if (!device_cfg) {
        print("virtio_pci_device: no DEVICE_CFG capability found\n");
        return;
    }

    if (!notify_cfg) {
        print("virtio_pci_device: no NOTIFY_CFG capability found\n");
        return;
    }

    if (!check_cap(dev, *common_cfg))
        return;

    if (common_cfg->length < sizeof(virtio_pci_common_cfg)) {
        print("virtio_pci_device: common cfg length was {}, expected at least {}\n",
              common_cfg->length, sizeof(virtio_pci_common_cfg));
        return;
    }

    if (!check_cap(dev, *device_cfg))
        return;

    if (!check_cap(dev, *notify_cfg))
        return;

    if (notify_cfg->cap_len < sizeof(virtio_pci_notify_cap) + sizeof(pci_capability)) {
        print("virtio_pci_device: notify capability length was {}, expected at least {}\n",
              notify_cfg->cap_len, sizeof(virtio_pci_notify_cap) + sizeof(pci_capability));
        return;
    }

    // Qemu's MSI-X won't work unless old-fashioned IRQs disabled
    dev.header->command |= PCI_COMMAND_INTX_DISABLE;

    common = (volatile virtio_pci_common_cfg*)(dev.bar[common_cfg->bar].data() + common_cfg->offset);

    common->device_status = 0;

    common->device_status = VIRTIO_DEVICE_STATUS_ACKNOWLEDGE;
    common->device_status |= VIRTIO_DEVICE_STATUS_DRIVER;

    uint64_t device_features = 0;

    common->device_feature_select = 0;
    device_features = common->device_feature;

    common->device_feature_select = 1;
    device_features |= ((uint64_t)common->device_feature) << 32;

    negotiated_features = device_features & supported_features;

    common->driver_feature_select = 0;
    common->driver_feature = (uint32_t)negotiated_features;

    common->driver_feature_select = 1;
    common->driver_feature = (uint32_t)(negotiated_features >> 32);

    common->device_status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;

    if (!(common->device_status & VIRTIO_DEVICE_STATUS_FEATURES_OK)) {
        print("found_virtio_pci: device rejected FEATURES_OK status bit\n");
        return;
    }

    // enable MSI-X
    dev.msix->message_control |= 0x8000;

    for (uint16_t i = 0; i < num_queues; i++) {
        if (!add_virtqueue(i))
            return;
    }

    initialized = true;
}

virt_queue::virt_queue() {
    for (unsigned int i = 0; i < QUEUE_SIZE; i++) {
        events[i] = create_event(true);
    }
}

virt_queue::~virt_queue() {
    static const size_t NUM_PAGES = bytes_to_pages(sizeof(virtq<QUEUE_SIZE>));

    queue->~virtq<QUEUE_SIZE>();

    auto va = (uintptr_t)queue;

    for (unsigned int i = 0; i < NUM_PAGES; i++) {
        unmap_page(va + (i << PAGE_SHIFT));
    }

    free_va(va, NUM_PAGES);

    free_folio(queue_pa);
}

void virt_queue::isr() {
    lock_guard lock(submit_lock);

    auto idx = queue->used.idx % QUEUE_SIZE;

    while (old_used_idx != idx) {
        lengths[queue->used.ring[old_used_idx].id] = queue->used.ring[old_used_idx].len;

        set_event(static_cast<event&>(events[queue->used.ring[old_used_idx].id].get()));

        old_used_idx++;
        if (old_used_idx == QUEUE_SIZE)
            old_used_idx = 0;
    }
}

virtio_pci_device::~virtio_pci_device() {
    while (!is_list_empty(virtqueues)) {
        auto& vq = containing_record<virt_queue>(virtqueues.next);

        remove_entry_list(vq.le);

        delete &vq;
    }
}

void virtio_pci_device::finalize() {
    common->device_status |= VIRTIO_DEVICE_STATUS_DRIVER_OK;
}

virtio_mmio_device::virtio_mmio_device(volatile virtio_mmio_regs_v1& regs,
                                       uint64_t supported_features,
                                       uint16_t num_queues, void* config) : config(config), var_regs(&regs) {
    regs.Status = 0;

    regs.GuestPageSize = PAGE_SIZE;

    regs.Status = VIRTIO_DEVICE_STATUS_ACKNOWLEDGE;
    regs.Status |= VIRTIO_DEVICE_STATUS_DRIVER;

    uint64_t device_features = 0;

    regs.HostFeaturesSel = 0;
    device_features = regs.HostFeatures;

    regs.HostFeaturesSel = 1;
    device_features |= ((uint64_t)regs.HostFeatures) << 32;

    negotiated_features = device_features & supported_features;

    regs.GuestFeaturesSel = 0;
    regs.GuestFeatures = (uint32_t)negotiated_features;

    regs.GuestFeaturesSel = 1;
    regs.GuestFeatures = (uint32_t)(negotiated_features >> 32);

    regs.Status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;

    if (!(regs.Status & VIRTIO_DEVICE_STATUS_FEATURES_OK)) {
        print("virtio_mmio_device: device rejected FEATURES_OK status bit\n");
        return;
    }

    for (uint16_t i = 0; i < num_queues; i++) {
        if (!add_virtqueue(i))
            return;
    }

    for (auto le = virtqueues.next; le != &virtqueues; le = le->next) {
        auto& q = containing_record<virt_queue>(le);

        q.notification_addr = &regs.QueueNotify;
    }

    initialized = true;
}

virtio_mmio_device::virtio_mmio_device(volatile virtio_mmio_regs_v2& regs,
                                       uint64_t supported_features,
                                       uint16_t num_queues, void* config) : config(config), var_regs(&regs) {
    regs.Status = 0;

    regs.Status = VIRTIO_DEVICE_STATUS_ACKNOWLEDGE;
    regs.Status |= VIRTIO_DEVICE_STATUS_DRIVER;

    uint64_t device_features = 0;

    regs.DeviceFeaturesSel = 0;
    device_features = regs.DeviceFeatures;

    regs.DeviceFeaturesSel = 1;
    device_features |= ((uint64_t)regs.DeviceFeatures) << 32;

    negotiated_features = device_features & supported_features;

    regs.DriverFeaturesSel = 0;
    regs.DriverFeatures = (uint32_t)negotiated_features;

    regs.DriverFeaturesSel = 1;
    regs.DriverFeatures = (uint32_t)(negotiated_features >> 32);

    regs.Status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;

    if (!(regs.Status & VIRTIO_DEVICE_STATUS_FEATURES_OK)) {
        print("virtio_mmio_device: device rejected FEATURES_OK status bit\n");
        return;
    }

    for (uint16_t i = 0; i < num_queues; i++) {
        if (!add_virtqueue(i))
            return;
    }

    for (auto le = virtqueues.next; le != &virtqueues; le = le->next) {
        auto& q = containing_record<virt_queue>(le);

        q.notification_addr = &regs.QueueNotify;
    }

    initialized = true;
}

virtio_mmio_device::~virtio_mmio_device() {
    while (!is_list_empty(virtqueues)) {
        auto& vq = containing_record<virt_queue>(virtqueues.next);

        remove_entry_list(vq.le);

        delete &vq;
    }
}

bool virtio_mmio_device::add_virtqueue(uint16_t num) {
    if (holds_alternative<volatile virtio_mmio_regs_v2*>(var_regs)) {
        volatile auto& regs = **get_if<volatile virtio_mmio_regs_v2*>(&var_regs);

        regs.QueueSel = num;

        if (regs.QueueSizeMax < QUEUE_SIZE) {
            print("virtio_mmio_device::add_virtqueue: max size for queue {} was {}, wanted at least {}\n",
                  regs.QueueSel, regs.QueueSizeMax, QUEUE_SIZE);
            return false;
        }

        regs.QueueSize = QUEUE_SIZE;
    } else {
        volatile auto& regs = **get_if<volatile virtio_mmio_regs_v1*>(&var_regs);

        regs.QueueSel = num;

        if (regs.QueuePFN != 0) {
            print("virtio_mmio_device::add_virtqueue: QueuePFN was {:x}, expected 0\n",
                  regs.QueuePFN);
            return false;
        }

        if (regs.QueueSizeMax < QUEUE_SIZE) {
            print("virtio_mmio_device::add_virtqueue: max size for queue {} was {}, wanted at least {}\n",
                  regs.QueueSel, regs.QueueSizeMax, QUEUE_SIZE);
            return false;
        }

        regs.QueueSize = QUEUE_SIZE;
    }

    static const size_t NUM_PAGES = bytes_to_pages(sizeof(virtq<QUEUE_SIZE>));

    auto pa = allocate_folio(bytes_to_folio_order(sizeof(virtq<QUEUE_SIZE>)));
    if (!pa) {
        print("allocate folio failed\n");
        return false;
    }

    auto va = allocate_va(NUM_PAGES);
    if (!va) {
        print("allocate_va failed\n");
        free_folio(pa);
        return false;
    }

    for (unsigned int i = 0; i < NUM_PAGES; i++) {
        if (!map_page(pa + (i << PAGE_SHIFT), va + (i << PAGE_SHIFT), mapping_type::no_cache)) {
            print("map_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                unmap_page(va + (j << PAGE_SHIFT));
            }

            free_va(va, NUM_PAGES);
            free_folio(pa);

            return false;
        }
    }

    auto vq = new virt_queue;
    if (!vq) {
        print("new failed\n");

        for (unsigned int i = 0; i < NUM_PAGES; i++) {
            unmap_page(va + (i << PAGE_SHIFT));
        }

        free_va(va, NUM_PAGES);
        free_folio(pa);
        return false;
    }

    auto& queue = *(virtq<QUEUE_SIZE>*)va;

    memset(&queue, 0, sizeof(queue));

    vq->num = num;
    vq->queue = &queue;
    vq->queue_pa = pa;

    if (holds_alternative<volatile virtio_mmio_regs_v2*>(var_regs)) {
        volatile auto& regs = **get_if<volatile virtio_mmio_regs_v2*>(&var_regs);

        regs.QueueDescLow = pa & 0xffffffff;
        regs.QueueDescHigh = (uint32_t)(pa >> 32);

        auto queue_driver = pa + ((uintptr_t)&queue.available - (uintptr_t)&queue);

        regs.QueueDriverLow = queue_driver & 0xffffffff;
        regs.QueueDriverHigh = (uint32_t)(queue_driver >> 32);

        auto queue_device = pa + ((uintptr_t)&queue.used - (uintptr_t)&queue);

        regs.QueueDeviceLow = queue_device & 0xffffffff;
        regs.QueueDeviceHigh = (uint32_t)(queue_device >> 32);

        regs.QueueReady = 1;
    } else {
        volatile auto& regs = **get_if<volatile virtio_mmio_regs_v1*>(&var_regs);

        regs.QueueAlign = PAGE_SIZE;

        // FIXME - (theoretically) need to make sure located within first 2^(32+PAGE_SHIFT) bytes
        regs.QueuePFN = (uint32_t)(pa >> PAGE_SHIFT);
    }

    insert_tail_list(virtqueues, vq->le);

    return true;
}

void virtio_mmio_device::finalize() {
    if (holds_alternative<volatile virtio_mmio_regs_v2*>(var_regs))
        (*get_if<volatile virtio_mmio_regs_v2*>(&var_regs))->Status |= VIRTIO_DEVICE_STATUS_DRIVER_OK;
    else
        (*get_if<volatile virtio_mmio_regs_v1*>(&var_regs))->Status |= VIRTIO_DEVICE_STATUS_DRIVER_OK;
}

bool virtio_mmio_device::isr() {
    if (holds_alternative<volatile virtio_mmio_regs_v2*>(var_regs)) {
        if (!((*get_if<volatile virtio_mmio_regs_v2*>(&var_regs))->InterruptStatus & 1))
            return false;
    } else {
        if (!((*get_if<volatile virtio_mmio_regs_v1*>(&var_regs))->InterruptStatus & 1))
            return false;
    }

    for (auto le = virtqueues.next; le != &virtqueues; le = le->next) {
        auto& vq = containing_record<virt_queue>(le);

        vq.isr();
    }

    if (holds_alternative<volatile virtio_mmio_regs_v2*>(var_regs))
        (*get_if<volatile virtio_mmio_regs_v2*>(&var_regs))->InterruptACK |= 1;
    else
        (*get_if<volatile virtio_mmio_regs_v1*>(&var_regs))->InterruptACK |= 1;

    return true;
}

static bool virtio_mmio_isr_func(void* ctx) {
    auto& v = *(virtio_mmio_device*)ctx;

    return v.isr();
}

static bool init_virtio_mmio2(volatile virtio_mmio_regs_v2& regs,
                              void* config, uint32_t irq) {
    if (regs.MagicValue != VIRTIO_MMIO_MAGIC_VALUE) {
        print("init_virtio_mmio2: incorrect MagicValue (found {:x}, expected {:x})\n",
              regs.MagicValue, VIRTIO_MMIO_MAGIC_VALUE);
        return false;
    }

    auto func = [&config, &irq](volatile auto& regs) -> bool {
        if (regs.DeviceID == 0) {
            // not printing an error, as per the spec
            return false;
        }

        for (auto le = virtio_mmio_types.next; le != &virtio_mmio_types; le = le->next) {
            auto& r = containing_record<virtio_mmio_recognizer>(le);

            if (r.device_id != regs.DeviceID)
                continue;

            auto v = new virtio_mmio_device(regs, r.supported_features,
                                            r.num_queues, config);

            if (!v) {
                print("new failed\n");
                return false;
            }

            if (!v->initialized) {
                print("init_virtio_mmio2: virtio_mmio_device initialization failed\n");
                delete v;
                return false;
            }

            auto local_apic_id = *(volatile uint32_t*)&local_apic[0x20];

            if (!add_irq_handler(irq, local_apic_id, virtio_mmio_isr_func, v)) {
                print("add_irq_handler failed\n");
                delete v;
                return false;
            }

            if (!r.func(*v)) {
                // FIXME - remove IRQ handler
                delete v;
                return false;
            }

            return true;
        }

        print("init_virtio_mmio2: device type {} not recognized\n", regs.DeviceID);

        return false;
    };

    switch (regs.Version) {
        case 1:
            return func(*(volatile virtio_mmio_regs_v1*)&regs);

        case 2:
            return func(regs);

        default:
            print("init_virtio_mmio2: unsupported version {}\n", regs.Version);
            return false;
    }
}

static void init_virtio_mmio(const ACPI_RESOURCE_FIXED_MEMORY32& mem,
                             uint32_t irq) {
    if (mem.AddressLength < sizeof(virtio_mmio_regs_v2)) {
        print("init_virtio_mmio: AddressLength was {:x}, expected at least {:x}\n",
              mem.AddressLength, sizeof(virtio_mmio_regs_v2));
        return;
    }

    uint64_t first_page = mem.Address >> PAGE_SHIFT;
    uint64_t last_page = (mem.Address + mem.AddressLength - 1) >> PAGE_SHIFT;
    auto num_pages = (uint32_t)(last_page - first_page + 1);

    auto va = allocate_va(num_pages);
    if (!va) {
        print("allocate_va failed\n");
        return;
    }

    for (uint32_t i = 0; i < num_pages; i++) {
        if (!map_page((first_page + i) << PAGE_SHIFT, va + (i << PAGE_SHIFT), mapping_type::no_cache)) {
            print("map_page failed\n");

            for (uint32_t j = 0; j < i; j++) {
                unmap_page(va + (j << PAGE_SHIFT));
            }

            free_va(va, num_pages);
            return;
        }
    }

    auto& regs = *(volatile virtio_mmio_regs_v2*)(va + (mem.Address & (PAGE_SIZE - 1)));

    void* config = (uint8_t*)&regs + sizeof(virtio_mmio_regs_v2);

    if (!init_virtio_mmio2(regs, config, irq)) {
        for (uint32_t i = 0; i < num_pages; i++) {
            unmap_page(va + (i << PAGE_SHIFT));
        }
        free_va(va, num_pages);
    }
}

static void found_virtio_mmio(const acpi_device& d) {
    optional<ACPI_RESOURCE_FIXED_MEMORY32> mem;
    optional<uint32_t> irq;

    for (auto le = d.resources.next; le != &d.resources; le = le->next) {
        auto& r = containing_record<acpi_device_resource>(le);

        if (r.type == acpi_device_resource_type::fixed_memory32) {
            auto& r2 = static_cast<acpi_device_resource_fixed_memory32&>(r);

            mem = r2.fixed_memory32;
        } else if (r.type == acpi_device_resource_type::irq) {
            auto& r2 = static_cast<acpi_device_resource_irq&>(r);

            irq = r2.irq.Interrupt;
        } else if (r.type == acpi_device_resource_type::extended_irq) {
            auto& r2 = static_cast<acpi_device_resource_extended_irq&>(r);

            irq = r2.extended_irq.Interrupt;
        }
    }

    if (!mem.has_value()) {
        print("virtio_mmio ACPI device found, but memory address not recognized\n");
        return;
    }

    if (!irq.has_value()) {
        print("virtio_mmio ACPI device found, but IRQ not recognized\n");
        return;
    }

    init_virtio_mmio(*mem, *irq);
}
