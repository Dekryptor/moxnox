module;

#include "kernel.h"
#include "pci.h"

export module virtio;

import print;
import bsod;
import pci;

static const uint16_t PCI_VENDOR_ID_REDHAT = 0x1af4;

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

export void found_virtio_pci(pci_device& dev);

static const struct {
    uint16_t vendor_id;
    uint16_t device_id;
    void (*func)(pci_device&);
} mod_pci_ids[] = {
    { PCI_VENDOR_ID_REDHAT, 0x1003, found_virtio_pci },
    { PCI_VENDOR_ID_REDHAT, 0x1043, found_virtio_pci },
    { 0x0000, 0x0000, nullptr }
};

static void init_virtio_pci() {
    for (unsigned int i = 0; mod_pci_ids[i].vendor_id != 0x0000; i++) {
        auto ent = new pci_recognizer;
        if (!ent)
            bsod("new failed in initialization");

        ent->vendor_id = mod_pci_ids[i].vendor_id;
        ent->device_id = mod_pci_ids[i].device_id;
        ent->func = mod_pci_ids[i].func;

        insert_tail_list(pci_ids, ent->le);
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

void found_virtio_pci(pci_device& dev) {
    virtio_pci_cap* common_cfg = nullptr;

    for (auto le = dev.vendor_specific_caps.next; le != &dev.vendor_specific_caps; le = le->next) {
        auto& vs = containing_record<vendor_specific_cap>(le);
        auto& cap = *(virtio_pci_cap*)vs.ptr;

        // FIXME - check cap.cap_len

        // FIXME - NOTIFY_CFG, DEVICE_CFG, ISR_CFG

        if (cap.cfg_type == VIRTIO_PCI_CAP::COMMON_CFG && !common_cfg)
            common_cfg = &cap;
    }

    if (!common_cfg) {
        print("found_virtio_pci: no COMMON_CFG capability found\n");
        return;
    }

    if (common_cfg->length < sizeof(virtio_pci_common_cfg)) {
        print("found_virtio_pci: common cfg length was {}, expected at least {}\n",
              common_cfg->length, sizeof(virtio_pci_common_cfg));
        return;
    }

    if (common_cfg->bar >= dev.bar.size()) {
        print("found_virtio_pci: common cfg BAR was {}, expected 0 to {}\n",
              common_cfg->bar, dev.bar.size());
        return;
    }

    if (common_cfg->offset >= dev.bar[common_cfg->bar].size()) {
        print("found_virtio_pci: common cfg started at {:x}, beyond end of BAR {} ({:x})\n",
              common_cfg->offset, common_cfg->bar, dev.bar[common_cfg->bar].size() - 1);
        return;
    }

    if (common_cfg->offset + common_cfg->length >= dev.bar[common_cfg->bar].size()) {
        print("found_virtio_pci: common cfg ended at {:x}, beyond end of BAR {} ({:x})\n",
              common_cfg->offset + common_cfg->length - 1, common_cfg->bar,
              dev.bar[common_cfg->bar].size() - 1);
        return;
    }

    auto& common = *(volatile virtio_pci_common_cfg*)(dev.bar[common_cfg->bar].data() + common_cfg->offset);

    print("FIXME: found_virtio_pci\n"); // FIXME

    common.device_status = VIRTIO_DEVICE_STATUS_ACKNOWLEDGE;
    common.device_status |= VIRTIO_DEVICE_STATUS_DRIVER;

    uint64_t device_features = 0;

    common.device_feature_select = 0;
    device_features = common.device_feature;

    common.device_feature_select = 1;
    device_features |= ((uint64_t)common.device_feature) << 32;

    print("device features = {:016x}\n", device_features);

    uint64_t driver_features = 0; // FIXME - return what we're supporting

    common.driver_feature_select = 0;
    common.driver_feature = (uint32_t)driver_features;

    common.driver_feature_select = 1;
    common.driver_feature = (uint32_t)(driver_features >> 32);

    common.device_status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;

    if (!(common.device_status & VIRTIO_DEVICE_STATUS_FEATURES_OK)) {
        print("found_virtio_pci: device rejected FEATURES_OK status bit\n");
        return;
    }

    // FIXME - set up virtqueues etc.

    common.device_status |= VIRTIO_DEVICE_STATUS_DRIVER_OK;
}
