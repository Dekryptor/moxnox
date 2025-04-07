module;

#include "kernel.h"
#include "pci.h"
#include "virtio.h"
#include "spinlock.h"
#include "handle.h"

export module virtio;

import print;
import bsod;
import pci;
import mem;
import config;
import interrupts;
import semaphore;
import event;
import chardev;
import format;

using namespace std;

static const uint16_t PCI_VENDOR_ID_REDHAT = 0x1af4;

// FIXME - allow dynamic queue size (maximum supported by device?)
const size_t QUEUE_SIZE = 32;

static const uint64_t supported_features = VIRTIO_CONSOLE_F_EMERG_WRITE;

export void found_virtio_pci(pci_device& dev);

static struct pci_recognizer mod_pci_ids[] = {
    { {}, PCI_VENDOR_ID_REDHAT, 0x1003, found_virtio_pci },
    { {}, PCI_VENDOR_ID_REDHAT, 0x1043, found_virtio_pci },
};

static void init_virtio_pci() {
    for (unsigned int i = 0; i < sizeof(mod_pci_ids) / sizeof(*mod_pci_ids); i++) {
        insert_tail_list(pci_ids, mod_pci_ids[i].le);
    }
}

static module_init init(init_virtio_pci);

class virt_queue {
public:
    virt_queue();
    ~virt_queue();
    nt_status write(span<const uint8_t> msg, uint64_t& written_bytes);
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

class virtio_pci_device {
public:
    virtio_pci_device(pci_device& dev);
    ~virtio_pci_device();
    void finalize();

    pci_device& dev;
    bool initialized = false;
    list_entry virtqueues = { &virtqueues, &virtqueues };

private:
    bool add_virtqueue(uint16_t num);

    volatile virtio_pci_common_cfg* common;
    virtio_pci_notify_cap* notify_cfg = nullptr;
};

class virtio_console : public virtio_pci_device, public chardev {
public:
    virtio_console(pci_device& dev);

    nt_status read(span<uint8_t> s, uint64_t& read_bytes) override;
    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override;

    nt_status write(string_view sv, uint64_t& written_bytes) {
        return write(span((uint8_t*)sv.data(), sv.length()), written_bytes);
    }

private:
    virt_queue* send_queue = nullptr;
};

static atomic<uint32_t> virtio_console_num = 0;

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

static void virtio_isr_func(void* ctx) {
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

    auto int_num = add_isr(virtio_isr_func, vq);
    if (int_num == 0) {
        print("add_isr failed\n");
        delete vq;
        return false;
    }

    write_volatile64(&common->queue_desc, pa);
    write_volatile64(&common->queue_driver, pa + ((uintptr_t)&queue.available - (uintptr_t)&queue));
    write_volatile64(&common->queue_device, pa + ((uintptr_t)&queue.used - (uintptr_t)&queue));

    uint16_t msix_vector = 0; // FIXME

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

        allocated_slots &= ~(1 << (uint32_t)slot);
    }

    release_semaphore(static_cast<semaphore&>(sem.get()));
}

nt_status virt_queue::write(span<const uint8_t> msg, uint64_t& written_bytes) {
    // FIXME - do without copying

    auto buf = malloc(msg.size());
    if (!buf) {
        print("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    memcpy(buf, msg.data(), msg.size());

    auto slot = get_slot();

    queue->buffers[slot].addr = (phys_memory_pfn(buf) << PAGE_SHIFT) + ((uintptr_t)buf & (PAGE_SIZE - 1));
    queue->buffers[slot].len = (uint32_t)msg.size();
    queue->buffers[slot].flags = 0;

    {
        lock_guard lock(submit_lock);

        queue->available.ring[queue->available.idx % QUEUE_SIZE] = slot;

        barrier();
        __asm __volatile("sfence" ::: "memory");

        queue->available.idx++;
    }

    *notification_addr = 1;

    // FIXME - make interruptible
    wait_for_object(events[slot].get(), false);

    written_bytes = lengths[slot];

    release_slot(slot);

    free(buf);

    return nt_status::SUCCESS;
}

virtio_pci_device::virtio_pci_device(pci_device& dev) : dev(dev) {
    virtio_pci_cap* common_cfg = nullptr;
    virtio_pci_cap* device_cfg = nullptr;

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

    if (device_cfg->length < sizeof(virtio_console_config)) {
        print("virtio_pci_device: device cfg length was {}, expected at least {}\n",
              device_cfg->length, sizeof(virtio_console_config));
        return;
    }

    if (!check_cap(dev, *notify_cfg))
        return;

    if (notify_cfg->cap_len < sizeof(virtio_pci_notify_cap) + sizeof(pci_capability)) {
        print("virtio_pci_device: notify capability length was {}, expected at least {}\n",
              notify_cfg->cap_len, sizeof(virtio_pci_notify_cap) + sizeof(pci_capability));
        return;
    }

    // Qemu's MSI-X won't work unless bus master is set and old-fashioned IRQs disabled
    dev.header->command |= PCI_COMMAND_INTX_DISABLE | PCI_COMMAND_MASTER;

    common = (volatile virtio_pci_common_cfg*)(dev.bar[common_cfg->bar].data() + common_cfg->offset);

    common->device_status = 0;

    common->device_status = VIRTIO_DEVICE_STATUS_ACKNOWLEDGE;
    common->device_status |= VIRTIO_DEVICE_STATUS_DRIVER;

    uint64_t device_features = 0;

    common->device_feature_select = 0;
    device_features = common->device_feature;

    common->device_feature_select = 1;
    device_features |= ((uint64_t)common->device_feature) << 32;

    uint64_t driver_features = device_features & supported_features;

    common->driver_feature_select = 0;
    common->driver_feature = (uint32_t)driver_features;

    common->driver_feature_select = 1;
    common->driver_feature = (uint32_t)(driver_features >> 32);

    common->device_status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;

    if (!(common->device_status & VIRTIO_DEVICE_STATUS_FEATURES_OK)) {
        print("found_virtio_pci: device rejected FEATURES_OK status bit\n");
        return;
    }

    // enable MSI-X
    dev.msix->message_control |= 0x8000;

    if (!add_virtqueue(1))
        return;

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

virtio_console::virtio_console(pci_device& dev) : virtio_pci_device(dev) {
    for (auto le = virtqueues.next; le != &virtqueues; le = le->next) {
        auto& vq = containing_record<virt_queue>(virtqueues.next);

        if (vq.num == 1) {
            send_queue = &vq;
            break;
        }
    }
}

nt_status virtio_console::read(span<uint8_t> s, uint64_t& read_bytes) {
    print("FIXME - virtio_console::read\n"); // FIXME

    return nt_status::NOT_IMPLEMENTED;
}

nt_status virtio_console::write(span<const uint8_t> s, uint64_t& written_bytes) {
    return send_queue->write(s, written_bytes);
}

void found_virtio_pci(pci_device& dev) {
    auto v = new virtio_console(dev);
    if (!v) {
        print("new failed\n");
        return;
    }

    object_ref ref{*v};

    if (!v->initialized)
        return;

    v->finalize();

    // FIXME - make sure devices are numbered consistently

    {
        char s[10];
        span sp(s, sizeof(s));

        sp = format(sp, "hvc{}", virtio_console_num.fetch_add(1));

        name_object(*v, string_view(s, sp.data() - s));
    }

    uint64_t written_bytes;

    v->write("hello world\n", written_bytes); // FIXME
    v->write("(and again...)\n", written_bytes); // FIXME
}
