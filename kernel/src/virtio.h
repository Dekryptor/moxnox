#pragma once

static const uint8_t VIRTIO_DEVICE_STATUS_ACKNOWLEDGE = 0x01;
static const uint8_t VIRTIO_DEVICE_STATUS_DRIVER = 0x02;
static const uint8_t VIRTIO_DEVICE_STATUS_DRIVER_OK = 0x04;
static const uint8_t VIRTIO_DEVICE_STATUS_FEATURES_OK = 0x08;
static const uint8_t VIRTIO_DEVICE_STATUS_DEVICE_NEEDS_RESET = 0x40;
static const uint8_t VIRTIO_DEVICE_STATUS_FAILED = 0x80;

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
