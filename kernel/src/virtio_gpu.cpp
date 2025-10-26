module;

#include <span>
#include <atomic>
#include <string_view>
#include <ioctl.h>
#include "handle.h"

export module virtio_gpu;

import print;
import bsod;
import config;
import chardev;
import pipe;
import format;
import pci;
import virtio;
import list;
import module_init;
import power;
import mem;
import thread;

using namespace std;

const uint64_t VIRTIO_GPU_F_VIRGL = 1 << 0;
const uint64_t VIRTIO_GPU_F_EDID = 1 << 1;
const uint64_t VIRTIO_GPU_F_RESOURCE_UUID = 1 << 2;
const uint64_t VIRTIO_GPU_F_RESOURCE_BLOB = 1 << 3;
const uint64_t VIRTIO_GPU_F_CONTEXT_INIT = 1 << 4;

const uint64_t SUPPORTED_FEATURES = 0;

struct virtio_gpu_config {
    uint32_t events_read;
    uint32_t events_clear;
    uint32_t num_scanouts;
    uint32_t num_capsets;
} __attribute((packed));

static_assert(sizeof(virtio_gpu_config) == 16);

enum class VIRTIO_GPU_CMD : uint32_t {
    GET_DISPLAY_INFO = 0x0100,
    RESOURCE_CREATE_2D,
    RESOURCE_UNREF,
    SET_SCANOUT,
    RESOURCE_FLUSH,
    TRANSFER_TO_HOST_2D,
    RESOURCE_ATTACH_BACKING,
    RESOURCE_DETACH_BACKING,
    GET_CAPSET_INFO,
    GET_CAPSET,
    GET_EDID,
    RESOURCE_ASSIGN_UUID,
    RESOURCE_CREATE_BLOB,
    SET_SCANOUT_BLOB,

    CTX_CREATE = 0x0200,
    CTX_DESTROY,
    CTX_ATTACH_RESOURCE,
    CTX_DETACH_RESOURCE,
    RESOURCE_CREATE_3D,
    TRANSFER_TO_HOST_3D,
    TRANSFER_FROM_HOST_3D,
    SUBMIT_3D,
    RESOURCE_MAP_BLOB,
    RESOURCE_UNMAP_BLOB,

    UPDATE_CURSOR = 0x0300,
    MOVE_CURSOR,
};

enum class VIRTIO_GPU_RESP : uint32_t {
    OK_NODATA = 0x1100,
    OK_DISPLAY_INFO,
    OK_CAPSET_INFO,
    OK_CAPSET,
    OK_EDID,
    OK_RESOURCE_UUID,
    OK_MAP_INFO,

    ERR_UNSPEC = 0x1200,
    ERR_OUT_OF_MEMORY,
    ERR_INVALID_SCANOUT_ID,
    ERR_INVALID_RESOURCE_ID,
    ERR_INVALID_CONTEXT_ID,
    ERR_INVALID_PARAMETER,
};

static const uint32_t VIRTIO_GPU_FLAG_FENCE = 1 << 0;
static const uint32_t VIRTIO_GPU_FLAG_INFO_RING_IDX = 1 << 1;

struct virtio_gpu_ctrl_hdr {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint8_t ring_idx;
    uint8_t padding[3];
} __attribute((packed));

static_assert(sizeof(virtio_gpu_ctrl_hdr) == 24);

static const size_t VIRTIO_GPU_MAX_SCANOUTS = 16;

struct virtio_gpu_rect {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} __attribute((packed));

static_assert(sizeof(virtio_gpu_rect) == 16);

struct virtio_gpu_resp_display_info {
    virtio_gpu_ctrl_hdr hdr;
    struct {
        virtio_gpu_rect r;
        uint32_t enabled;
        uint32_t flags;
    } pmodes[VIRTIO_GPU_MAX_SCANOUTS];
} __attribute((packed));

static_assert(sizeof(virtio_gpu_resp_display_info) == 408);

static const uint16_t PCI_VENDOR_ID_REDHAT = 0x1af4;

export void found_virtio_pci_gpu(pci_device& dev);

static pci_recognizer mod_pci_ids[] = {
    { {}, PCI_VENDOR_ID_REDHAT, 0x1050, true, found_virtio_pci_gpu },
};

static bool found_virtio_mmio_gpu(virtio_mmio_device& v);

static virtio_mmio_recognizer mod_virtio_mmio_types = {
    {}, 16, SUPPORTED_FEATURES, 2, found_virtio_mmio_gpu
};

static void init_virtio_pci() {
    for (unsigned int i = 0; i < sizeof(mod_pci_ids) / sizeof(*mod_pci_ids); i++) {
        insert_tail_list(pci_ids, mod_pci_ids[i].le);
    }

    insert_tail_list(virtio_mmio_types, mod_virtio_mmio_types.le);
}

static module_init module_init_virtio_gpu(init_virtio_pci);

template<typename T>
class virtio_gpu : public chardev {
public:
    virtio_gpu(pci_device& d);
    virtio_gpu(T& t);

    ~virtio_gpu() {
        delete &dev;
    }

    nt_status read(span<uint8_t>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status write(span<const uint8_t>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    T& dev;

private:
    virt_queue* control_queue = nullptr;
    virt_queue* cursor_queue = nullptr;
};

template<>
virtio_gpu<virtio_pci_device>::virtio_gpu(pci_device& d) : dev(*new virtio_pci_device(d, SUPPORTED_FEATURES, 2)) {
    if (dev.device_cfg->length < sizeof(virtio_gpu_config)) {
        print("virtio_gpu: device cfg length was {}, expected at least {}\n",
              dev.device_cfg->length, sizeof(virtio_gpu_config));
        return;
    }

    for (auto le = dev.virtqueues.next; le != &dev.virtqueues; le = le->next) {
        auto& vq = containing_record<virt_queue>(le);

        if (vq.num == 0)
            control_queue = &vq;
        else if (vq.num == 1)
            cursor_queue = &vq;
    }
}

template<>
virtio_gpu<virtio_mmio_device>::virtio_gpu(virtio_mmio_device& d) : dev(d) {
    for (auto le = dev.virtqueues.next; le != &dev.virtqueues; le = le->next) {
        auto& vq = containing_record<virt_queue>(le);

        if (vq.num == 0)
            control_queue = &vq;
        else if (vq.num == 1)
            cursor_queue = &vq;
    }
}

static atomic<uint32_t> virtio_gpu_num = 0;

void found_virtio_pci_gpu(pci_device& dev) {
    print("FIXME - found_virtio_pci_gpu\n");

    auto v = new virtio_gpu<virtio_pci_device>(dev);
    if (!v) {
        print("new failed\n");
        return;
    }

    object_ref ref{*v};

    if (!v->dev.initialized)
        return;

    v->dev.finalize();

    {
        char s[10];
        span sp(s, sizeof(s));

        sp = format(sp, "virtgpu{}", virtio_gpu_num.fetch_add(1));

        auto status = name_object(*v, string_view(s, (size_t)(sp.data() - s)));
        if (!success(status))
            print("name_object returned {}\n", status);
    }
}

static bool found_virtio_mmio_gpu(virtio_mmio_device& dev) {
    print("FIXME - found_virtio_mmio_gpu\n");

    auto pv = new virtio_gpu<virtio_mmio_device>(dev);
    if (!pv) {
        print("new failed\n");
        return false;
    }

    object_ref ref{*pv};

    auto& v = *pv;

    if (!v.dev.initialized)
        return false;

    v.dev.finalize();

    {
        char s[10];
        span sp(s, sizeof(s));

        sp = format(sp, "virtgpu{}", virtio_gpu_num.fetch_add(1));

        auto status = name_object(v, string_view(s, (size_t)(sp.data() - s)));
        if (!success(status)) {
            print("name_object returned {}\n", status);
            return false;
        }
    }

    return true;
}
