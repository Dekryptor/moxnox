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

virtio_block::virtio_block(pci_device& dev) : virtio_pci_device(dev, 0, 1) {
    // FIXME - device_cfg

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

    v.sector_size = 512; // FIXME - VIRTIO_BLK_F_BLK_SIZE
    // FIXME - set v.size

    print("found /dev/{}: block size {}, capacity {}\n", string_view(v.volume_name, v.volume_name_len),
          byte_size(v.sector_size), byte_size(v.size));

    v.probe();
}
