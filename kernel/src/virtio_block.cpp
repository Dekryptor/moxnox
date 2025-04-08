module;

#include "kernel.h"
#include "spinlock.h"
#include "handle.h"
import rbtree;
#include "page_cache.h"

export module virtio_block;

import print;
import pci;
import format;
import volume;
import virtio;

using namespace std;

static const uint64_t VIRTIO_BLK_F_BARRIER = 1 << 0;
static const uint64_t VIRTIO_BLK_F_SIZE_MAX = 1 << 1;
static const uint64_t VIRTIO_BLK_F_SEG_MAX = 1 << 2;
static const uint64_t VIRTIO_BLK_F_GEOMETRY = 1 << 4;
static const uint64_t VIRTIO_BLK_F_RO = 1 << 5;
static const uint64_t VIRTIO_BLK_F_BLK_SIZE = 1 << 6;
static const uint64_t VIRTIO_BLK_F_SCSI = 1 << 7;
static const uint64_t VIRTIO_BLK_F_FLUSH = 1 << 9;
static const uint64_t VIRTIO_BLK_F_TOPOLOGY = 1 << 10;
static const uint64_t VIRTIO_BLK_F_CONFIG_WCE = 1 << 11;
static const uint64_t VIRTIO_BLK_F_MQ = 1 << 12;
static const uint64_t VIRTIO_BLK_F_DISCARD = 1 << 13;
static const uint64_t VIRTIO_BLK_F_WRITE_ZEROES = 1 << 14;
static const uint64_t VIRTIO_BLK_F_LIFETIME = 1 << 15;
static const uint64_t VIRTIO_BLK_F_SECURE_ERASE = 1 << 16;
static const uint64_t VIRTIO_BLK_F_ZONED = 1 << 17;

static const uint64_t SUPPORTED_FEATURES = VIRTIO_BLK_F_BLK_SIZE;

struct virtio_blk_config {
    uint64_t capacity;
    uint32_t size_max;
    uint32_t seg_max;
    struct {
        uint16_t cylinders;
        uint8_t heads;
        uint8_t sectors;
    } geometry;
    uint32_t blk_size;
    struct {
        uint8_t physical_block_exp;
        uint8_t alignment_offset;
        uint16_t min_io_size;
        uint32_t opt_io_size;
    } topology;
    uint8_t writeback;
    uint8_t unused0;
    uint16_t num_queues;
    uint32_t max_discard_sectors;
    uint32_t max_discard_seg;
    uint32_t discard_sector_alignment;
    uint32_t max_write_zeroes_sectors;
    uint32_t max_write_zeroes_seg;
    uint8_t write_zeroes_may_unmap;
    uint8_t unused1[3];
    uint32_t max_secure_erase_sectors;
    uint32_t max_secure_erase_seg;
    uint32_t secure_erase_sector_alignment;
    struct {
        uint32_t zone_sectors;
        uint32_t max_open_zones;
        uint32_t max_active_zones;
        uint32_t max_append_sectors;
        uint32_t write_granularity;
        uint8_t model;
        uint8_t unused2[3];
    } zoned;
} __attribute((packed));

static_assert(sizeof(virtio_blk_config) == 96);

static const uint16_t PCI_VENDOR_ID_REDHAT = 0x1af4;

class virtio_block : public virtio_pci_device, public volume {
public:
    virtio_block(pci_device& dev);

    void release() override {
        if (--refcount == 0)
            delete this;
    }

    void flush() override;
    size_t dio_read(uint64_t page_num, uint64_t num_pages,
                    pfn_iterator& it) override;
    size_t dio_write(uint64_t page_num, uint64_t num_pages,
                     pfn_iterator& it, bool fua) override;

    unsigned int dev_num;

private:
    virt_queue* queue = nullptr;
};

static atomic<uint32_t> virtio_block_num = 0;

export void found_virtio_pci_block(pci_device& dev);

static pci_recognizer mod_pci_ids[] = {
    { {}, PCI_VENDOR_ID_REDHAT, 0x1001, found_virtio_pci_block },
    { {}, PCI_VENDOR_ID_REDHAT, 0x1042, found_virtio_pci_block },
};

static void init_virtio_pci() {
    for (unsigned int i = 0; i < sizeof(mod_pci_ids) / sizeof(*mod_pci_ids); i++) {
        insert_tail_list(pci_ids, mod_pci_ids[i].le);
    }
}

static module_init init(init_virtio_pci);

virtio_block::virtio_block(pci_device& dev) : virtio_pci_device(dev, SUPPORTED_FEATURES, 1) {
    if (device_cfg->length < sizeof(virtio_blk_config)) {
        print("virtio_block: device cfg length was {}, expected at least {}\n",
              device_cfg->length, sizeof(virtio_blk_config));
        return;
    }

    for (auto le = virtqueues.next; le != &virtqueues; le = le->next) {
        auto& vq = containing_record<virt_queue>(le);

        if (vq.num == 0) {
            queue = &vq;
            break;
        }
    }
}

void virtio_block::flush() {
    print("FIXME - virtio_block::flush\n"); // FIXME
}

size_t virtio_block::dio_read(uint64_t page_num, uint64_t num_pages,
                              pfn_iterator& it) {
    print("FIXME - virtio_block::dio_read\n"); // FIXME

    return 0;
}

size_t virtio_block::dio_write(uint64_t page_num, uint64_t num_pages,
                               pfn_iterator& it, bool fua) {
    print("FIXME - virtio_block::dio_write\n"); // FIXME

    return 0;
}

void found_virtio_pci_block(pci_device& dev) {
    auto pv = new virtio_block(dev);
    if (!pv) {
        print("new failed\n");
        return;
    }

    object_ref ref{*pv};

    auto& v = *pv;

    if (!v.initialized)
        return;

    v.finalize();

    // FIXME - make sure devices are numbered consistently
    v.dev_num = virtio_block_num.fetch_add(1);

    if (v.dev_num < 26) {
        char name[4];

        name[0] = 'v';
        name[1] = 'd';
        name[2] = (char)(v.dev_num + 'a');
        name[3] = 0;

        if (!name_object(v, name)) {
            print("name_object failed\n");
            return;
        }

        v.volume_name_len = strlen(name);
        v.volume_name = (char*)malloc(v.volume_name_len);
        if (!v.volume_name) {
            print("malloc failed\n");
            return;
        }

        memcpy(v.volume_name, name, v.volume_name_len);
    }

    auto& cfg = *(volatile virtio_blk_config*)(dev.bar[v.device_cfg->bar].data() + v.device_cfg->offset);

    if (v.negotiated_features & VIRTIO_BLK_F_BLK_SIZE)
        v.sector_size = cfg.blk_size;
    else
        v.sector_size = 512;

    // FIXME - set v.size

    print("found /dev/{}: block size {}, capacity {}\n", string_view(v.volume_name, v.volume_name_len),
          byte_size(v.sector_size), byte_size(v.size));

    v.probe();
}
