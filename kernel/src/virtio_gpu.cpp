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

const uint64_t SUPPORTED_FEATURES = VIRTIO_RING_F_INDIRECT_DESC;

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

const size_t VIRTIO_GPU_MAX_SCANOUTS = 16;

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

enum class VIRTIO_GPU_FORMAT : uint32_t {
    B8G8R8A8_UNORM = 1,
    B8G8R8X8_UNORM = 2,
    A8R8G8B8_UNORM = 3,
    X8R8G8B8_UNORM = 4,
    R8G8B8A8_UNORM = 67,
    X8B8G8R8_UNORM = 68,
    A8B8G8R8_UNORM = 121,
    R8G8B8X8_UNORM = 134,
};

struct virtio_gpu_resource_create_2d {
    virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    VIRTIO_GPU_FORMAT format;
    uint32_t width;
    uint32_t height;
} __attribute((packed));

static_assert(sizeof(virtio_gpu_resource_create_2d) == 40);

struct virtio_gpu_mem_entry {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
} __attribute((packed));

static_assert(sizeof(virtio_gpu_mem_entry) == 16);

struct virtio_gpu_resource_attach_backing {
    virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
    virtio_gpu_mem_entry entries[1];
} __attribute((packed));

static_assert(offsetof(virtio_gpu_resource_attach_backing, entries) == 32);

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

    void start();

    bool get_display_info(virtio_gpu_resp_display_info& disp_info);
    optional<uint32_t> create_resource(VIRTIO_GPU_FORMAT format,
                                       uint32_t width, uint32_t height);
    bool attach_backing(uint32_t resource, span<uint8_t> buf);

    T& dev;
    uint32_t num_scanouts;

private:
    virt_queue* control_queue = nullptr;
    virt_queue* cursor_queue = nullptr;
    atomic<uint32_t> resource_num = 1;
};

template<>
virtio_gpu<virtio_pci_device>::virtio_gpu(pci_device& d) : dev(*new virtio_pci_device(d, SUPPORTED_FEATURES, 2)) {
    if (dev.device_cfg->length < sizeof(virtio_gpu_config)) {
        print("virtio_gpu: device cfg length was {}, expected at least {}\n",
              dev.device_cfg->length, sizeof(virtio_gpu_config));
        return;
    }

    auto& cfg = *(volatile virtio_gpu_config*)(d.bar[dev.device_cfg->bar].data() + dev.device_cfg->offset);

    num_scanouts = cfg.num_scanouts;

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
    auto& cfg = *(volatile virtio_gpu_config*)(d.config);

    num_scanouts = cfg.num_scanouts;

    for (auto le = dev.virtqueues.next; le != &dev.virtqueues; le = le->next) {
        auto& vq = containing_record<virt_queue>(le);

        if (vq.num == 0)
            control_queue = &vq;
        else if (vq.num == 1)
            cursor_queue = &vq;
    }
}

template<>
consteval bool format_spec_valid<enum VIRTIO_GPU_RESP>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, enum VIRTIO_GPU_RESP r) {
    switch (r) {
        using enum VIRTIO_GPU_RESP;

        case OK_NODATA:
            return do_format_arg(sp, "", "OK_NODATA");
        case OK_DISPLAY_INFO:
            return do_format_arg(sp, "", "OK_DISPLAY_INFO");
        case OK_CAPSET_INFO:
            return do_format_arg(sp, "", "OK_CAPSET_INFO");
        case OK_CAPSET:
            return do_format_arg(sp, "", "OK_CAPSET");
        case OK_EDID:
            return do_format_arg(sp, "", "OK_EDID");
        case OK_RESOURCE_UUID:
            return do_format_arg(sp, "", "OK_RESOURCE_UUID");
        case OK_MAP_INFO:
            return do_format_arg(sp, "", "OK_MAP_INFO");
        case ERR_UNSPEC:
            return do_format_arg(sp, "", "ERR_UNSPEC");
        case ERR_OUT_OF_MEMORY:
            return do_format_arg(sp, "", "ERR_OUT_OF_MEMORY");
        case ERR_INVALID_SCANOUT_ID:
            return do_format_arg(sp, "", "ERR_INVALID_SCANOUT_ID");
        case ERR_INVALID_RESOURCE_ID:
            return do_format_arg(sp, "", "ERR_INVALID_RESOURCE_ID");
        case ERR_INVALID_CONTEXT_ID:
            return do_format_arg(sp, "", "ERR_INVALID_CONTEXT_ID");
        case ERR_INVALID_PARAMETER:
            return do_format_arg(sp, "", "ERR_INVALID_PARAMETER");
        default:
            return do_format_arg(sp, "", (uint32_t)r);
    }
}

template<>
consteval bool format_spec_valid<virtio_gpu_rect>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, const virtio_gpu_rect& r) {
    sp = do_format_arg(sp, "", "(");
    sp = do_format_arg(sp, "", r.x);
    sp = do_format_arg(sp, "", ",");
    sp = do_format_arg(sp, "", r.y);
    sp = do_format_arg(sp, "", ",");
    sp = do_format_arg(sp, "", r.width);
    sp = do_format_arg(sp, "", ",");
    sp = do_format_arg(sp, "", r.height);
    sp = do_format_arg(sp, "", ")");

    return sp;
}

template<typename T>
bool virtio_gpu<T>::get_display_info(virtio_gpu_resp_display_info& disp_info) {
    uint64_t read_bytes;

    auto req = (virtio_gpu_ctrl_hdr*)malloc(sizeof(virtio_gpu_ctrl_hdr));
    if (!req) {
        print("malloc failed\n");
        return false;
    }

    memset(req, 0, sizeof(*req));
    req->type = (uint32_t)VIRTIO_GPU_CMD::GET_DISPLAY_INFO;

    auto descs = (virtq_desc*)malloc(sizeof(virtq_desc) * 2);
    if (!descs) {
        print("malloc failed\n");
        free(req);
        return false;
    }

    descs[0].addr = (phys_memory_pfn(req) << PAGE_SHIFT) + ((uintptr_t)req & (PAGE_SIZE - 1));
    descs[0].len = sizeof(virtio_gpu_ctrl_hdr);
    descs[0].flags = VIRTQ_DESC_F_NEXT;
    descs[0].next = 1;

    descs[1].addr = (phys_memory_pfn(&disp_info) << PAGE_SHIFT) + ((uintptr_t)&disp_info & (PAGE_SIZE - 1));
    descs[1].len = sizeof(virtio_gpu_resp_display_info);
    descs[1].flags = VIRTQ_DESC_F_WRITE;
    descs[1].next = 0;

    auto status = control_queue->indirect(span((uint8_t*)descs, sizeof(virtq_desc) * 2),
                                          read_bytes);
    if (!success(status)) {
        free(descs);
        free(req);
        return false;
    }

    free(descs);
    free(req);

    auto resp_err = (enum VIRTIO_GPU_RESP)disp_info.hdr.type;

    if (resp_err != VIRTIO_GPU_RESP::OK_DISPLAY_INFO) {
        print("VIRTIO_GPU_CMD_GET_DISPLAY_INFO returned {}\n", resp_err);
        return false;
    }

    return true;
}

template<typename T>
optional<uint32_t> virtio_gpu<T>::create_resource(VIRTIO_GPU_FORMAT format,
                                                  uint32_t width,
                                                  uint32_t height) {
    uint64_t read_bytes;

    auto req = (virtio_gpu_resource_create_2d*)malloc(sizeof(virtio_gpu_resource_create_2d));
    if (!req) {
        print("malloc failed\n");
        return nullopt;
    }

    auto resp = (virtio_gpu_ctrl_hdr*)malloc(sizeof(virtio_gpu_ctrl_hdr));
    if (!resp) {
        print("malloc failed\n");
        free(req);
        return nullopt;
    }

    uint32_t num = resource_num.fetch_add(1);

    memset(req, 0, sizeof(*req));
    req->hdr.type = (uint32_t)VIRTIO_GPU_CMD::RESOURCE_CREATE_2D;
    req->resource_id = num;
    req->format = format;
    req->width = width;
    req->height = height;

    auto descs = (virtq_desc*)malloc(sizeof(virtq_desc) * 2);
    if (!descs) {
        print("malloc failed\n");
        free(resp);
        free(req);
        return nullopt;
    }

    descs[0].addr = (phys_memory_pfn(req) << PAGE_SHIFT) + ((uintptr_t)req & (PAGE_SIZE - 1));
    descs[0].len = sizeof(virtio_gpu_resource_create_2d);
    descs[0].flags = VIRTQ_DESC_F_NEXT;
    descs[0].next = 1;

    descs[1].addr = (phys_memory_pfn(resp) << PAGE_SHIFT) + ((uintptr_t)resp & (PAGE_SIZE - 1));
    descs[1].len = sizeof(virtio_gpu_ctrl_hdr);
    descs[1].flags = VIRTQ_DESC_F_WRITE;
    descs[1].next = 0;

    auto status = control_queue->indirect(span((uint8_t*)descs, sizeof(virtq_desc) * 2),
                                          read_bytes);
    if (!success(status)) {
        free(descs);
        free(resp);
        free(req);
        return nullopt;
    }

    free(descs);
    free(req);

    auto resp_err = (enum VIRTIO_GPU_RESP)resp->type;

    free(resp);

    if (resp_err != VIRTIO_GPU_RESP::OK_NODATA) {
        print("VIRTIO_GPU_CMD_RESOURCE_CREATE_2D returned {}\n", resp_err);
        return nullopt;
    }

    return num;
}

unsigned int calc_num_mem_entries(span<const uint8_t> buf) {
    unsigned int ret = 0;

    // buf must come from vmalloc

    while (!buf.empty()) {
        auto pa = va_to_pa_lookup((uintptr_t)buf.data());
        auto order = get_folio_order(pa);

        ret++;

        if (buf.size() <= (size_t)(1 << (PAGE_SHIFT + order)))
            break;

        buf = buf.subspan(1 << (PAGE_SHIFT + order));
    }

    return ret;
}

void populate_entries(span<virtio_gpu_mem_entry> entries, span<const uint8_t> buf) {
    while (!buf.empty()) {
        auto pa = va_to_pa_lookup((uintptr_t)buf.data());
        auto order = get_folio_order(pa);
        auto& e = entries.front();

        e.addr = pa;
        e.length = 1 << (PAGE_SHIFT + order);
        e.padding = 0;

        buf = buf.subspan(1 << (PAGE_SHIFT + order));
        entries = entries.subspan(1);
    }
}

void populate_descs(span<virtq_desc> descs, span<const uint8_t> buf) {
    uint16_t i = 0;

    while (!buf.empty()) {
        auto pa = va_to_pa_lookup((uintptr_t)buf.data());
        auto order = get_folio_order(pa);
        auto& d = descs.front();

        d.addr = pa;
        d.len = (uint32_t)min(buf.size(), (size_t)(1 << (PAGE_SHIFT + order)));
        d.flags = VIRTQ_DESC_F_NEXT;
        d.next = i + 1;

        if (buf.size() <= (size_t)(1 << (PAGE_SHIFT + order)))
            break;

        buf = buf.subspan(1 << (PAGE_SHIFT + order));
        descs = descs.subspan(1);
        i++;
    }
}

template<typename T>
bool virtio_gpu<T>::attach_backing(uint32_t resource, span<uint8_t> buf) {
    uint64_t read_bytes;

    auto num_entries = calc_num_mem_entries(buf);

    auto req_size = (uint32_t)(offsetof(virtio_gpu_resource_attach_backing, entries) +
                               (num_entries * sizeof(virtio_gpu_mem_entry)));

    auto req = (virtio_gpu_resource_attach_backing*)vmalloc(req_size);
    if (!req) {
        print("vmalloc failed\n");
        return false;
    }

    auto resp = (virtio_gpu_ctrl_hdr*)malloc(sizeof(virtio_gpu_ctrl_hdr));
    if (!resp) {
        print("malloc failed\n");
        vfree(req, req_size);
        return false;
    }

    memset(req, 0, req_size);
    req->hdr.type = (uint32_t)VIRTIO_GPU_CMD::RESOURCE_ATTACH_BACKING;
    req->resource_id = resource;
    req->nr_entries = num_entries;

    populate_entries(span(req->entries, num_entries), buf);

    auto req_entries = calc_num_mem_entries(span((uint8_t*)req, req_size));

    auto descs = (virtq_desc*)malloc(sizeof(virtq_desc) * (req_entries + 1));
    if (!descs) {
        print("malloc failed\n");
        free(resp);
        vfree(req, req_size);
        return false;
    }

    populate_descs(span(descs, num_entries), span((uint8_t*)req, req_size));

    descs[req_entries].addr = (phys_memory_pfn(resp) << PAGE_SHIFT) + ((uintptr_t)resp & (PAGE_SIZE - 1));
    descs[req_entries].len = sizeof(virtio_gpu_ctrl_hdr);
    descs[req_entries].flags = VIRTQ_DESC_F_WRITE;
    descs[req_entries].next = 0;

    auto status = control_queue->indirect(span((uint8_t*)descs, sizeof(virtq_desc) * (req_entries + 1)),
                                          read_bytes);
    if (!success(status)) {
        free(descs);
        free(resp);
        vfree(req, req_size);
        return false;
    }

    free(descs);
    vfree(req, req_size);

    auto resp_err = (enum VIRTIO_GPU_RESP)resp->type;

    free(resp);

    if (resp_err != VIRTIO_GPU_RESP::OK_NODATA) {
        print("VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING returned {}\n", resp_err);
        return false;
    }

    return true;
}

template<typename T>
void virtio_gpu<T>::start() {
    auto disp_info = (virtio_gpu_resp_display_info*)malloc(sizeof(virtio_gpu_resp_display_info));
    if (!disp_info) {
        print("malloc failed\n");
        return;
    }

    if (!get_display_info(*disp_info)) {
        print("virtio_gpu::get_display_info failed\n");
        free(disp_info);
        return;
    }

    for (size_t i = 0; i < num_scanouts; i++) {
        print("scanout {}: r {}, enabled {}, flags {}\n",
              i, disp_info->pmodes[i].r, disp_info->pmodes[i].enabled,
              disp_info->pmodes[i].flags);
    }

    if (!disp_info->pmodes[0].enabled) {
        print("virtio_gpu::start: scanout 0 not enabled\n");
        free(disp_info);
        return;
    }

    auto r = disp_info->pmodes[0].r;

    free(disp_info);

    // FIXME - check video cmdline option
    // FIXME - get EDID?

    // FIXME - multiple scanouts

    // FIXME - unref resource in destructor
    auto res = create_resource(VIRTIO_GPU_FORMAT::B8G8R8A8_UNORM, r.width,
                               r.height);
    if (!res.has_value()) {
        print("virtio_gpu::create_resource failed\n");
        return;
    }

    // FIXME - vfree in destructor
    auto backing = (uint32_t*)vmalloc(sizeof(uint32_t) * r.width * r.height);
    if (!backing) {
        print("vmalloc failed\n");
        // FIXME - unref resource
        return;
    }

    if (!attach_backing(*res, span((uint8_t*)backing, sizeof(uint32_t) * r.width * r.height))) {
        print("attach_backing failed\n");
        // FIXME - vfree
        // FIXME - unref resource
    }

    // FIXME - set scanout

    // FIXME - replace existing framebuffer if present
}

static atomic<uint32_t> virtio_gpu_num = 0;

void found_virtio_pci_gpu(pci_device& dev) {
    auto pv = new virtio_gpu<virtio_pci_device>(dev);
    if (!pv) {
        print("new failed\n");
        return;
    }

    object_ref ref{*pv};

    auto& v = *pv;

    if (!v.dev.initialized)
        return;

    v.dev.finalize();

    {
        char s[10];
        span sp(s, sizeof(s));

        sp = format(sp, "virtgpu{}", virtio_gpu_num.fetch_add(1));

        auto status = name_object(v, string_view(s, (size_t)(sp.data() - s)));
        if (!success(status))
            print("name_object returned {}\n", status);
    }

    v.start();
}

static bool found_virtio_mmio_gpu(virtio_mmio_device& dev) {
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

    v.start();

    return true;
}
