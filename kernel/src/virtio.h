#pragma once

static const uint8_t VIRTIO_DEVICE_STATUS_ACKNOWLEDGE = 0x01;
static const uint8_t VIRTIO_DEVICE_STATUS_DRIVER = 0x02;
static const uint8_t VIRTIO_DEVICE_STATUS_DRIVER_OK = 0x04;
static const uint8_t VIRTIO_DEVICE_STATUS_FEATURES_OK = 0x08;
static const uint8_t VIRTIO_DEVICE_STATUS_DEVICE_NEEDS_RESET = 0x40;
static const uint8_t VIRTIO_DEVICE_STATUS_FAILED = 0x80;

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

struct virtio_mmio_regs {
    uint32_t MagicValue;
    uint32_t Version;
    uint32_t DeviceID;
    uint32_t VendorID;
    uint32_t DeviceFeatures;
    uint32_t DeviceFeaturesSel;
    uint64_t padding1;
    uint32_t DriverFeatures;
    uint32_t DriverFeaturesSel;
    uint64_t padding2;
    uint32_t QueueSel;
    uint32_t QueueSizeMax;
    uint32_t QueueSize;
    uint64_t padding3;
    uint32_t QueueReady;
    uint64_t padding4;
    uint32_t QueueNotify;
    uint8_t padding5[12];
    uint32_t InterruptStatus;
    uint32_t InterruptACK;
    uint64_t padding6;
    uint32_t Status;
    uint8_t padding7[12];
    uint32_t QueueDescLow;
    uint32_t QueueDescHigh;
    uint64_t padding8;
    uint32_t QueueDriverLow;
    uint32_t QueueDriverHigh;
    uint64_t padding9;
    uint32_t QueueDeviceLow;
    uint32_t QueueDeviceHigh;
    uint32_t padding10;
    uint32_t SHMSel;
    uint32_t SHMLenLow;
    uint32_t SHMLenHigh;
    uint32_t SHMBaseLow;
    uint32_t SHMBaseHigh;
    uint32_t QueueReset;
    uint8_t padding11[56];
    uint32_t ConfigGeneration;
} __attribute((packed));

static_assert(sizeof(virtio_mmio_regs) == 0x100);
