module;

#include "kernel.h"
#include "spinlock.h"
#include "handle.h"
import rbtree;
#include "page_cache.h"
#include <shared_mutex>

export module virtio_block;

import print;
import pci;
import format;
import volume;
import virtio;
import config;
import mem;
import rwsem;

using namespace std;

const size_t SECTOR_SIZE = 512;

const uint64_t VIRTIO_BLK_F_BARRIER = 1 << 0;
const uint64_t VIRTIO_BLK_F_SIZE_MAX = 1 << 1;
const uint64_t VIRTIO_BLK_F_SEG_MAX = 1 << 2;
const uint64_t VIRTIO_BLK_F_GEOMETRY = 1 << 4;
const uint64_t VIRTIO_BLK_F_RO = 1 << 5;
const uint64_t VIRTIO_BLK_F_BLK_SIZE = 1 << 6;
const uint64_t VIRTIO_BLK_F_SCSI = 1 << 7;
const uint64_t VIRTIO_BLK_F_FLUSH = 1 << 9;
const uint64_t VIRTIO_BLK_F_TOPOLOGY = 1 << 10;
const uint64_t VIRTIO_BLK_F_CONFIG_WCE = 1 << 11;
const uint64_t VIRTIO_BLK_F_MQ = 1 << 12;
const uint64_t VIRTIO_BLK_F_DISCARD = 1 << 13;
const uint64_t VIRTIO_BLK_F_WRITE_ZEROES = 1 << 14;
const uint64_t VIRTIO_BLK_F_LIFETIME = 1 << 15;
const uint64_t VIRTIO_BLK_F_SECURE_ERASE = 1 << 16;
const uint64_t VIRTIO_BLK_F_ZONED = 1 << 17;

static const uint64_t SUPPORTED_FEATURES = VIRTIO_BLK_F_RO |
                                           VIRTIO_BLK_F_BLK_SIZE |
                                           VIRTIO_BLK_F_FLUSH |
                                           VIRTIO_RING_F_INDIRECT_DESC;

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

enum class VIRTIO_BLK_T : uint32_t {
    IN = 0,
    OUT = 1,
    FLUSH = 4,
    GET_ID = 8,
    GET_LIFETIME = 10,
    DISCARD = 11,
    WRITE_ZEROES = 13,
    SECURE_ERASE = 14
};

struct virtio_blk_req {
    enum VIRTIO_BLK_T type;
    uint32_t reserved;
    uint64_t sector;
    // then data
    uint8_t status;
} __attribute((packed));

static_assert(sizeof(virtio_blk_req) == 17);

static const uint16_t PCI_VENDOR_ID_REDHAT = 0x1af4;

template<typename T>
class virtio_block : public volume {
public:
    virtio_block(pci_device& d);
    virtio_block(T& t);
    ~virtio_block();

    void release() override {
        if (--refcount == 0)
            delete this;
    }

    nt_status flush() override;
    nt_status dio_read(uint64_t page_num, uint64_t num_pages,
                       pfn_iterator& it, uint64_t& written_pages) override;
    nt_status dio_write(uint64_t page_num, uint64_t num_pages,
                        pfn_iterator& it, uint64_t& written_pages,
                        bool fua) override;

    T& dev;
    unsigned int dev_num;

private:
    virt_queue* queue = nullptr;
    rwsem flush_sem;
};

static atomic<uint32_t> virtio_block_num = 0;

static void found_virtio_pci_block(pci_device& dev);

static pci_recognizer mod_pci_ids[] = {
    { {}, PCI_VENDOR_ID_REDHAT, 0x1001, found_virtio_pci_block },
    { {}, PCI_VENDOR_ID_REDHAT, 0x1042, found_virtio_pci_block },
};

static bool found_virtio_mmio_block(virtio_mmio_device& v);

static virtio_mmio_recognizer mod_virtio_mmio_types = {
    {}, 2, SUPPORTED_FEATURES, 1, found_virtio_mmio_block
};

static void init_virtio_block() {
    for (unsigned int i = 0; i < sizeof(mod_pci_ids) / sizeof(*mod_pci_ids); i++) {
        insert_tail_list(pci_ids, mod_pci_ids[i].le);
    }

    insert_tail_list(virtio_mmio_types, mod_virtio_mmio_types.le);
}

static module_init module_init_virtio_block(init_virtio_block);

template<>
virtio_block<virtio_pci_device>::virtio_block(pci_device& d) : dev(*new virtio_pci_device(d, SUPPORTED_FEATURES, 1)) {
    if (dev.device_cfg->length < sizeof(virtio_blk_config)) {
        print("virtio_block: device cfg length was {}, expected at least {}\n",
              dev.device_cfg->length, sizeof(virtio_blk_config));
        return;
    }

    for (auto le = dev.virtqueues.next; le != &dev.virtqueues; le = le->next) {
        auto& vq = containing_record<virt_queue>(le);

        if (vq.num == 0) {
            queue = &vq;
            break;
        }
    }
}

template<>
virtio_block<virtio_mmio_device>::virtio_block(virtio_mmio_device& d) : dev(d) {
    for (auto le = dev.virtqueues.next; le != &dev.virtqueues; le = le->next) {
        auto& vq = containing_record<virt_queue>(le);

        if (vq.num == 0) {
            queue = &vq;
            break;
        }
    }
}

template<typename T>
virtio_block<T>::~virtio_block() {
    delete &dev;
}

template<typename T>
nt_status virtio_block<T>::flush() {
    if (!(dev.negotiated_features & VIRTIO_BLK_F_FLUSH))
        return nt_status::SUCCESS;

    auto req = (virtio_blk_req*)malloc(sizeof(virtio_blk_req));
    if (!req)
        return nt_status::INSUFFICIENT_RESOURCES;

    auto descs = (virtq_desc*)malloc(sizeof(virtq_desc) * 2);
    if (!descs) {
        free(req);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    req->type = VIRTIO_BLK_T::FLUSH;
    req->reserved = 0;
    req->sector = 0;

    descs[0].addr = (phys_memory_pfn(req) << PAGE_SHIFT) + ((uintptr_t)req & (PAGE_SIZE - 1));
    descs[0].len = offsetof(virtio_blk_req, status);
    descs[0].flags = VIRTQ_DESC_F_NEXT;
    descs[0].next = 1;

    descs[1].addr = descs[0].addr + descs[0].len;
    descs[1].len = sizeof(virtio_blk_req) - descs[0].len;
    descs[1].flags = VIRTQ_DESC_F_WRITE;
    descs[1].next = 0;

    nt_status status;

    {
        lock_guard lock(flush_sem);

        uint64_t buf_read_bytes;

        status = queue->indirect(span((uint8_t*)descs, sizeof(virtq_desc) * 2), buf_read_bytes);
    }

    if (!success(status)) {
        free(descs);
        free(req);
        return status;
    }

    if (req->status != 0) {
        print("virtio_block::flush: device returned status {:x}\n", req->status);
        free(descs);
        free(req);
        // FIXME - translate status into nt_status properly
        return nt_status::INTERNAL_ERROR;
    }

    free(descs);
    free(req);

    return nt_status::SUCCESS;
}

template<typename T>
nt_status virtio_block<T>::dio_read(uint64_t page_num, uint64_t num_pages,
                                    pfn_iterator& it, uint64_t& read_pages) {
    if (num_pages == 0) {
        read_pages = 0;
        return nt_status::SUCCESS;
    }

    // don't use more than 1 page for descriptor
    static const uint64_t MAX_PAGES = (PAGE_SIZE / sizeof(virtq_desc)) - 2;

    if (num_pages > MAX_PAGES)
        num_pages = MAX_PAGES;

    auto req = (virtio_blk_req*)malloc(sizeof(virtio_blk_req));
    if (!req)
        return nt_status::INSUFFICIENT_RESOURCES;

    auto descs = (virtq_desc*)malloc(sizeof(virtq_desc) * (num_pages + 2));
    if (!descs) {
        free(req);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    req->type = VIRTIO_BLK_T::IN;
    req->reserved = 0;
    req->sector = (page_num << PAGE_SHIFT) / SECTOR_SIZE;

    descs[0].addr = (phys_memory_pfn(req) << PAGE_SHIFT) + ((uintptr_t)req & (PAGE_SIZE - 1));
    descs[0].len = offsetof(virtio_blk_req, status);
    descs[0].flags = VIRTQ_DESC_F_NEXT;
    descs[0].next = 1;

    for (unsigned int i = 0; i < num_pages; i++) {
        if (i != 0)
            it++;

        descs[i + 1].addr = *it;
        descs[i + 1].len = PAGE_SIZE;
        descs[i + 1].flags = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;
        descs[i + 1].next = (uint16_t)(i + 2);
    }

    descs[num_pages + 1].addr = descs[0].addr + descs[0].len;
    descs[num_pages + 1].len = sizeof(virtio_blk_req) - descs[0].len;
    descs[num_pages + 1].flags = VIRTQ_DESC_F_WRITE;
    descs[num_pages + 1].next = 0;

    uint64_t buf_read_bytes;

    auto status = queue->indirect(span((uint8_t*)descs, sizeof(virtq_desc) * (num_pages + 2)), buf_read_bytes);
    if (!success(status)) {
        free(descs);
        free(req);
        return status;
    }

    if (req->status != 0) {
        print("virtio_block::dio_read: device returned status {:x}\n", req->status);
        free(descs);
        free(req);
        return nt_status::INTERNAL_ERROR; // FIXME - translate into sensible nt_status
    }

    read_pages = num_pages;

    free(descs);
    free(req);

    return nt_status::SUCCESS;
}

template<typename T>
nt_status virtio_block<T>::dio_write(uint64_t page_num, uint64_t num_pages,
                                     pfn_iterator& it, uint64_t& written_pages,
                                     bool fua) {
    if (dev.negotiated_features & VIRTIO_BLK_F_RO)
        return nt_status::MEDIA_WRITE_PROTECTED;

    if (num_pages == 0) {
        written_pages = 0;
        return nt_status::SUCCESS;
    }

    // don't use more than 1 page for descriptor
    static const uint64_t MAX_PAGES = (PAGE_SIZE / sizeof(virtq_desc)) - 2;

    if (num_pages > MAX_PAGES)
        num_pages = MAX_PAGES;

    auto req = (virtio_blk_req*)malloc(sizeof(virtio_blk_req));
    if (!req)
        return nt_status::INSUFFICIENT_RESOURCES;

    auto descs = (virtq_desc*)malloc(sizeof(virtq_desc) * (num_pages + 2));
    if (!descs) {
        free(req);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    req->type = VIRTIO_BLK_T::OUT;
    req->reserved = 0;
    req->sector = (page_num << PAGE_SHIFT) / SECTOR_SIZE;

    descs[0].addr = (phys_memory_pfn(req) << PAGE_SHIFT) + ((uintptr_t)req & (PAGE_SIZE - 1));
    descs[0].len = offsetof(virtio_blk_req, status);
    descs[0].flags = VIRTQ_DESC_F_NEXT;
    descs[0].next = 1;

    for (unsigned int i = 0; i < num_pages; i++) {
        if (i != 0)
            it++;

        descs[i + 1].addr = *it;
        descs[i + 1].len = PAGE_SIZE;
        descs[i + 1].flags = VIRTQ_DESC_F_NEXT;
        descs[i + 1].next = (uint16_t)(i + 2);
    }

    descs[num_pages + 1].addr = descs[0].addr + descs[0].len;
    descs[num_pages + 1].len = sizeof(virtio_blk_req) - descs[0].len;
    descs[num_pages + 1].flags = VIRTQ_DESC_F_WRITE;
    descs[num_pages + 1].next = 0;

    nt_status status;

    {
        shared_lock lock(flush_sem);

        uint64_t buf_read_bytes;

        status = queue->indirect(span((uint8_t*)descs, sizeof(virtq_desc) * (num_pages + 2)), buf_read_bytes);
    }

    if (!success(status)) {
        free(descs);
        free(req);
        return status;
    }

    if (req->status != 0) {
        print("virtio_block::dio_write: device returned status {:x}\n", req->status);
        free(descs);
        free(req);
        return nt_status::INTERNAL_ERROR; // FIXME - translate into sensible nt_status
    }

    written_pages = num_pages;

    free(descs);
    free(req);

    if (fua) {
        status = flush();
        if (!success(status)) {
            print("virtio_block::flush returned {}\n", status);
            return status;
        }
    }

    return nt_status::SUCCESS;
}

void found_virtio_pci_block(pci_device& dev) {
    auto pv = new virtio_block<virtio_pci_device>(dev);
    if (!pv) {
        print("new failed\n");
        return;
    }

    object_ref ref{*pv};

    auto& v = *pv;

    if (!v.dev.initialized)
        return;

    if (!(v.dev.negotiated_features & VIRTIO_RING_F_INDIRECT_DESC)) {
        print("found_virtio_pci_block: device does not support indirect descriptors\n");
        return;
    }

    v.dev.finalize();

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

    auto& cfg = *(volatile virtio_blk_config*)(dev.bar[v.dev.device_cfg->bar].data() + v.dev.device_cfg->offset);

    if (v.dev.negotiated_features & VIRTIO_BLK_F_BLK_SIZE)
        v.sector_size = cfg.blk_size;
    else
        v.sector_size = SECTOR_SIZE;

    v.size = cfg.capacity * SECTOR_SIZE;

    print("found /dev/{}: block size {}, capacity {}\n", string_view(v.volume_name, v.volume_name_len),
          byte_size(v.sector_size), byte_size(v.size));

    v.probe();
}

static bool found_virtio_mmio_block(virtio_mmio_device& dev) {
    auto pv = new virtio_block<virtio_mmio_device>(dev);
    if (!pv) {
        print("new failed\n");
        return false;
    }

    object_ref ref{*pv};

    auto& v = *pv;

    if (!v.dev.initialized)
        return false;

    if (!(v.dev.negotiated_features & VIRTIO_RING_F_INDIRECT_DESC)) {
        print("found_virtio_pci_block: device does not support indirect descriptors\n");
        return false;
    }

    v.dev.finalize();

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
            return false;
        }

        v.volume_name_len = strlen(name);
        v.volume_name = (char*)malloc(v.volume_name_len);
        if (!v.volume_name) {
            print("malloc failed\n");
            return false;
        }

        memcpy(v.volume_name, name, v.volume_name_len);
    }

    auto& cfg = *(volatile virtio_blk_config*)(dev.config);

    if (v.dev.negotiated_features & VIRTIO_BLK_F_BLK_SIZE)
        v.sector_size = cfg.blk_size;
    else
        v.sector_size = SECTOR_SIZE;

    v.size = cfg.capacity * SECTOR_SIZE;

    print("found /dev/{}: block size {}, capacity {}\n", string_view(v.volume_name, v.volume_name_len),
          byte_size(v.sector_size), byte_size(v.size));

    v.probe();

    return true;
}
