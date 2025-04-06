module;

#include "kernel.h"
#include "pci.h"

export module virtio;

import print;
import bsod;
import pci;
import mem;
import config;

using namespace std;

static const uint16_t PCI_VENDOR_ID_REDHAT = 0x1af4;

// FIXME - allow dynamic queue size (maximum supported by device?)
static const size_t QUEUE_SIZE = 32;

static const uint8_t VIRTIO_DEVICE_STATUS_ACKNOWLEDGE = 0x01;
static const uint8_t VIRTIO_DEVICE_STATUS_DRIVER = 0x02;
static const uint8_t VIRTIO_DEVICE_STATUS_DRIVER_OK = 0x04;
static const uint8_t VIRTIO_DEVICE_STATUS_FEATURES_OK = 0x08;
static const uint8_t VIRTIO_DEVICE_STATUS_DEVICE_NEEDS_RESET = 0x40;
static const uint8_t VIRTIO_DEVICE_STATUS_FAILED = 0x80;

static const uint64_t VIRTIO_CONSOLE_F_SIZE = 1 << 0;
static const uint64_t VIRTIO_CONSOLE_F_MULTIPORT = 1 << 1;
static const uint64_t VIRTIO_CONSOLE_F_EMERG_WRITE = 1 << 2;

static const uint64_t VIRTIO_RING_F_INDIRECT_DESC = 1 << 28;
static const uint64_t VIRTIO_RING_F_EVENT_IDX = 1 << 29;
static const uint64_t VIRTIO_F_VERSION_1 = (uint64_t)1 << 32;
static const uint64_t VIRTIO_F_ACCESS_PLATFORM = (uint64_t)1 << 33;
static const uint64_t VIRTIO_F_RING_PACKED = (uint64_t)1 << 34;
static const uint64_t VIRTIO_F_IN_ORDER = (uint64_t)1 << 35;
static const uint64_t VIRTIO_F_ORDER_PLATFORM = (uint64_t)1 << 36;
static const uint64_t VIRTIO_F_SR_IOV = (uint64_t)1 << 37;
static const uint64_t VIRTIO_F_NOTIFICATION_DATA = (uint64_t)1 << 38;
static const uint64_t VIRTIO_F_RING_RESET = (uint64_t)1 << 40;

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

enum class VIRTIO_PCI_CAP : uint8_t {
    COMMON_CFG = 1,
    NOTIFY_CFG = 2,
    ISR_CFG = 3,
    DEVICE_CFG = 4,
    PCI_CFG = 5,
    SHARED_MEMORY_CFG = 8,
    VENDOR_CFG = 9
};

struct virtio_pci_cap {
    uint8_t cap_len;
    enum VIRTIO_PCI_CAP cfg_type;
    uint8_t bar;
    uint8_t id;
    uint8_t padding[2];
    uint32_t offset;
    uint32_t length;
} __attribute((packed));

static_assert(sizeof(virtio_pci_cap) == 14);

struct virtio_pci_notify_cap : virtio_pci_cap {
    uint32_t notify_off_multiplier;
} __attribute((packed));

static_assert(sizeof(virtio_pci_notify_cap) == 18);

struct virtio_pci_common_cfg {
    uint32_t device_feature_select;
    uint32_t device_feature;
    uint32_t driver_feature_select;
    uint32_t driver_feature;
    uint16_t config_msix_vector;
    uint16_t num_queues;
    uint8_t device_status;
    uint8_t config_generation;
    uint16_t queue_select;
    uint16_t queue_size;
    uint16_t queue_msix_vector;
    uint16_t queue_enable;
    uint16_t queue_notify_off;
    uint64_t queue_desc;
    uint64_t queue_driver;
    uint64_t queue_device;
    uint16_t queue_notif_config_data;
    uint16_t queue_reset;
    uint16_t admin_queue_index;
    uint16_t admin_queue_num;
} __attribute((packed));

static_assert(sizeof(virtio_pci_common_cfg) == 64);

struct virtio_console_config {
    uint16_t cols;
    uint16_t rows;
    uint32_t max_nr_ports;
    uint32_t emerg_wr;
} __attribute((packed));

static_assert(sizeof(virtio_console_config) == 12);

static const uint8_t VIRTQ_DESC_F_NEXT = 1;
static const uint8_t VIRTQ_DESC_F_WRITE = 2;
static const uint8_t VIRTQ_DESC_F_INDIRECT = 4;

struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute((packed));

static_assert(sizeof(virtq_desc) == 16);

static const uint16_t VIRTQ_AVAIL_F_NO_INTERRUPT = 1;

template<unsigned N>
struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[N];
    uint16_t used_event;
} __attribute((packed));

struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute((packed));

static_assert(sizeof(virtq_used_elem) == 8);

static const uint16_t VIRTQ_USED_F_NO_NOTIFY = 1;

template<unsigned N>
struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    virtq_used_elem ring[N];
    uint16_t avail_event;
} __attribute((packed));

template<unsigned N>
struct virtq {
    virtq_desc buffers[N];
    virtq_avail<N> available;
    uint8_t padding[4096 - (N * sizeof(virtq_desc)) - sizeof(virtq_avail<N>)];
    virtq_used<N> used;
} __attribute((packed));

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

static bool init_virtqueues(volatile virtio_pci_common_cfg& common,
                            virtq<QUEUE_SIZE>*& addr) {
    common.queue_select = 1;

    if (common.queue_size < QUEUE_SIZE) {
        print("init_virtqueues: size for queue {} was {}, wanted at least {}\n",
              common.queue_select, common.queue_size, QUEUE_SIZE);
        return false;
    }

    common.queue_size = QUEUE_SIZE;

    auto pa = allocate_folio(bytes_to_folio_order(sizeof(virtq<QUEUE_SIZE>)));
    if (!pa) {
        print("allocate folio failed\n");
        return false;
    }

    auto& queue = *(virtq<QUEUE_SIZE>*)phys_address(pa);

    addr = &queue;

    memset(&queue, 0, sizeof(queue));

    // FIXME - set up interrupts (queue_msix_vector)

    write_volatile64(&common.queue_desc, pa);
    write_volatile64(&common.queue_driver, pa + ((uintptr_t)&queue.available - (uintptr_t)&queue));
    write_volatile64(&common.queue_device, pa + ((uintptr_t)&queue.used - (uintptr_t)&queue));

    common.queue_notify_off = 0;

    common.queue_enable = 1;

    return true;
}

static void write_virtqueue(virtq<QUEUE_SIZE>& queue,
                            virtio_pci_notify_cap& notify_cfg, pci_device& dev,
                            string_view msg) {
    uint16_t idx = 0; // FIXME

    auto buf = malloc(msg.size());
    if (!buf) {
        print("malloc failed\n");
        return;
    }

    memcpy(buf, msg.data(), msg.size());

    queue.buffers[idx].addr = (phys_memory_pfn(buf) << PAGE_SHIFT) + ((uintptr_t)buf & (PAGE_SIZE - 1));
    queue.buffers[idx].len = (uint32_t)msg.size();
    queue.buffers[idx].flags = 0;

    queue.available.ring[queue.available.idx % QUEUE_SIZE] = idx;
    queue.available.idx++;

    barrier();
    __asm __volatile("sfence" ::: "memory");

    uint32_t queue_notify_off = 0; // FIXME

    auto addr = (volatile uint32_t*)(dev.bar[notify_cfg.bar].data() +
                                     notify_cfg.offset +
                                     (queue_notify_off * notify_cfg.notify_off_multiplier));
    print("addr = {:x}\n", (uintptr_t)addr);

    *addr = 1;
    // FIXME - send notification
}

void found_virtio_pci(pci_device& dev) {
    virtio_pci_cap* common_cfg = nullptr;
    virtio_pci_cap* device_cfg = nullptr;
    virtio_pci_notify_cap* notify_cfg = nullptr;

    for (auto le = dev.vendor_specific_caps.next; le != &dev.vendor_specific_caps; le = le->next) {
        auto& vs = containing_record<vendor_specific_cap>(le);
        auto& cap = *(virtio_pci_cap*)vs.ptr;

        // FIXME - ISR_CFG

        if (cap.cfg_type == VIRTIO_PCI_CAP::COMMON_CFG && !common_cfg)
            common_cfg = &cap;
        else if (cap.cfg_type == VIRTIO_PCI_CAP::DEVICE_CFG && !device_cfg)
            device_cfg = &cap;
        else if (cap.cfg_type == VIRTIO_PCI_CAP::NOTIFY_CFG && !notify_cfg)
            notify_cfg = reinterpret_cast<virtio_pci_notify_cap*>(&cap);
    }

    if (!common_cfg) {
        print("found_virtio_pci: no COMMON_CFG capability found\n");
        return;
    }

    if (!device_cfg) {
        print("found_virtio_pci: no DEVICE_CFG capability found\n");
        return;
    }

    if (!notify_cfg) {
        print("found_virtio_pci: no NOTIFY_CFG capability found\n");
        return;
    }

    if (!check_cap(dev, *common_cfg))
        return;

    if (common_cfg->length < sizeof(virtio_pci_common_cfg)) {
        print("found_virtio_pci: common cfg length was {}, expected at least {}\n",
              common_cfg->length, sizeof(virtio_pci_common_cfg));
        return;
    }

    if (!check_cap(dev, *device_cfg))
        return;

    if (device_cfg->length < sizeof(virtio_console_config)) {
        print("found_virtio_pci: device cfg length was {}, expected at least {}\n",
              device_cfg->length, sizeof(virtio_console_config));
        return;
    }

    if (!check_cap(dev, *notify_cfg))
        return;

    if (notify_cfg->cap_len < sizeof(virtio_pci_notify_cap) + sizeof(pci_capability)) {
        print("found_virtio_pci: notify capability length was {}, expected at least {}\n",
              notify_cfg->cap_len, sizeof(virtio_pci_notify_cap) + sizeof(pci_capability));
        return;
    }

    auto& common = *(volatile virtio_pci_common_cfg*)(dev.bar[common_cfg->bar].data() + common_cfg->offset);

    print("FIXME: found_virtio_pci\n"); // FIXME

    common.device_status = 0;

    common.device_status = VIRTIO_DEVICE_STATUS_ACKNOWLEDGE;
    common.device_status |= VIRTIO_DEVICE_STATUS_DRIVER;

    uint64_t device_features = 0;

    common.device_feature_select = 0;
    device_features = common.device_feature;

    common.device_feature_select = 1;
    device_features |= ((uint64_t)common.device_feature) << 32;

    print("device features = {:016x}\n", device_features);

    uint64_t driver_features = device_features & supported_features;

    common.driver_feature_select = 0;
    common.driver_feature = (uint32_t)driver_features;

    common.driver_feature_select = 1;
    common.driver_feature = (uint32_t)(driver_features >> 32);

    common.device_status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;

    if (!(common.device_status & VIRTIO_DEVICE_STATUS_FEATURES_OK)) {
        print("found_virtio_pci: device rejected FEATURES_OK status bit\n");
        return;
    }

    virtq<QUEUE_SIZE>* virtqueue0;

    if (!init_virtqueues(common, virtqueue0))
        return;

    common.device_status |= VIRTIO_DEVICE_STATUS_DRIVER_OK;

    write_virtqueue(*virtqueue0, *notify_cfg, dev, "hello world\n");
}
