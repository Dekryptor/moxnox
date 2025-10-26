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

static const uint16_t PCI_VENDOR_ID_REDHAT = 0x1af4;

export void found_virtio_pci_gpu(pci_device& dev);

static pci_recognizer mod_pci_ids[] = {
    { {}, PCI_VENDOR_ID_REDHAT, 0x1050, true, found_virtio_pci_gpu },
};

static bool found_virtio_mmio_gpu(virtio_mmio_device& v);

static virtio_mmio_recognizer mod_virtio_mmio_types = {
    {}, 16, SUPPORTED_FEATURES, 1, found_virtio_mmio_gpu
};

static void init_virtio_pci() {
    for (unsigned int i = 0; i < sizeof(mod_pci_ids) / sizeof(*mod_pci_ids); i++) {
        insert_tail_list(pci_ids, mod_pci_ids[i].le);
    }

    insert_tail_list(virtio_mmio_types, mod_virtio_mmio_types.le);
}

static module_init module_init_virtio_gpu(init_virtio_pci);

void found_virtio_pci_gpu(pci_device& dev) {
    print("FIXME - found_virtio_pci_gpu\n");
}

static bool found_virtio_mmio_gpu(virtio_mmio_device& dev) {
    print("FIXME - found_virtio_mmio_gpu\n");

    return false;
}
