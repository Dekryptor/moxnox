module;

#include "kernel.h"
#include <ioctl.h>
#include "handle.h"

export module virtio_console;

import print;
import bsod;
import config;
import chardev;
import format;
import serial;
import pci;
import virtio;

using namespace std;

const uint64_t VIRTIO_CONSOLE_F_SIZE = 1 << 0;
const uint64_t VIRTIO_CONSOLE_F_MULTIPORT = 1 << 1;
const uint64_t VIRTIO_CONSOLE_F_EMERG_WRITE = 1 << 2;

const uint64_t SUPPORTED_FEATURES = VIRTIO_CONSOLE_F_EMERG_WRITE;

// FIXME - avoiding -Wtemplate-names-tu-local warning
const auto _IOCTL_MOXNOX_CONSOLE_SET_FOREGROUND = IOCTL_MOXNOX_CONSOLE_SET_FOREGROUND;

struct virtio_console_config {
    uint16_t cols;
    uint16_t rows;
    uint32_t max_nr_ports;
    uint32_t emerg_wr;
} __attribute((packed));

static_assert(sizeof(virtio_console_config) == 12);

static const uint16_t PCI_VENDOR_ID_REDHAT = 0x1af4;

export void found_virtio_pci_console(pci_device& dev);

static pci_recognizer mod_pci_ids[] = {
    { {}, PCI_VENDOR_ID_REDHAT, 0x1003, true, found_virtio_pci_console },
    { {}, PCI_VENDOR_ID_REDHAT, 0x1043, true, found_virtio_pci_console },
};

static bool found_virtio_mmio_console(virtio_mmio_device& v);

static virtio_mmio_recognizer mod_virtio_mmio_types = {
    {}, 3, SUPPORTED_FEATURES, 2, found_virtio_mmio_console
};

static void init_virtio_pci() {
    for (unsigned int i = 0; i < sizeof(mod_pci_ids) / sizeof(*mod_pci_ids); i++) {
        insert_tail_list(pci_ids, mod_pci_ids[i].le);
    }

    insert_tail_list(virtio_mmio_types, mod_virtio_mmio_types.le);
}

static module_init module_init_virtio_console(init_virtio_pci);

template<typename T>
class virtio_console : public chardev, public serial_console {
public:
    virtio_console(pci_device& d);
    virtio_console(T& t);
    ~virtio_console();

    nt_status read(span<uint8_t> s, uint64_t& read_bytes) override;
    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override;

    nt_status write(string_view sv, uint64_t& written_bytes) {
        return write(span((uint8_t*)sv.data(), sv.length()), written_bytes);
    }

    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t>, uintptr_t&) override;

    T& dev;

private:
    virt_queue* recv_queue = nullptr;
    virt_queue* send_queue = nullptr;
};

static atomic<uint32_t> virtio_console_num = 0;

template<>
virtio_console<virtio_pci_device>::virtio_console(pci_device& d) : dev(*new virtio_pci_device(d, SUPPORTED_FEATURES, 2)) {
    if (dev.device_cfg->length < sizeof(virtio_console_config)) {
        print("virtio_console: device cfg length was {}, expected at least {}\n",
              dev.device_cfg->length, sizeof(virtio_console_config));
        return;
    }

    for (auto le = dev.virtqueues.next; le != &dev.virtqueues; le = le->next) {
        auto& vq = containing_record<virt_queue>(le);

        if (vq.num == 0)
            recv_queue = &vq;
        else if (vq.num == 1)
            send_queue = &vq;
    }
}

template<>
virtio_console<virtio_mmio_device>::virtio_console(virtio_mmio_device& d) : dev(d) {
    for (auto le = dev.virtqueues.next; le != &dev.virtqueues; le = le->next) {
        auto& vq = containing_record<virt_queue>(le);

        if (vq.num == 0)
            recv_queue = &vq;
        else if (vq.num == 1)
            send_queue = &vq;
    }
}

template<typename T>
virtio_console<T>::~virtio_console() {
    delete &dev;
}

template<typename T>
nt_status virtio_console<T>::read(span<uint8_t> s, uint64_t& read_bytes) {
    auto buf_size = min(s.size(), PAGE_SIZE);
    auto buf = (uint8_t*)malloc(buf_size);
    if (!buf) {
        print("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    uint64_t buf_read_bytes;

    auto status = recv_queue->read(span(buf, buf_size), buf_read_bytes);
    if (!success(status)) {
        free(buf);
        return status;
    }

    read_bytes = 0;

    status = vt100_recv(span(buf, buf_read_bytes), [&s, &read_bytes](span<const uint8_t> s2) {
        memcpy(s.data(), s2.data(), s2.size());
        s = span(s.data() + s2.size(), s.size() - s2.size());
        read_bytes += s2.size();

        return nt_status::SUCCESS;
    });

    return status;
}

template<typename T>
nt_status virtio_console<T>::write(span<const uint8_t> s, uint64_t& written_bytes) {
    if (shutting_down)
        return nt_status::SYSTEM_SHUTDOWN;

    if (in_bsod) {
        if (!(dev.negotiated_features & VIRTIO_CONSOLE_F_EMERG_WRITE))
            return nt_status::SYSTEM_SHUTDOWN;

        volatile virtio_console_config* v;

        if constexpr (is_same_v<T, virtio_pci_device>)
            v = (volatile virtio_console_config*)(dev.dev.bar[dev.device_cfg->bar].data() + dev.device_cfg->offset);
        else
            v = (volatile virtio_console_config*)dev.config;

        for (auto c : s) {
            v->emerg_wr = c;
        }

        written_bytes = s.size();

        return nt_status::SUCCESS;
    }

    // FIXME - only so we don't do an overly large malloc (should be able to do this without copying)
    if (s.size() > PAGE_SIZE)
        s = span(s.data(), PAGE_SIZE);

    auto status = send_queue->write(s);

    if (success(status))
        written_bytes = s.size();

    return status;
}

template<typename T>
nt_status virtio_console<T>::ioctl(uint32_t code, span<const uint8_t> inbuf,
                                span<uint8_t>, uintptr_t&) {
    switch (code) {
        case _IOCTL_MOXNOX_CONSOLE_SET_FOREGROUND:
            return console_set_foreground(inbuf);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

void found_virtio_pci_console(pci_device& dev) {
    auto v = new virtio_console<virtio_pci_device>(dev);
    if (!v) {
        print("new failed\n");
        return;
    }

    object_ref ref{*v};

    if (!v->dev.initialized)
        return;

    v->dev.finalize();

    // FIXME - make sure devices are numbered consistently

    {
        char s[10];
        span sp(s, sizeof(s));

        sp = format(sp, "hvc{}", virtio_console_num.fetch_add(1));

        name_object(*v, string_view(s, (size_t)(sp.data() - s)));
    }
}

static bool found_virtio_mmio_console(virtio_mmio_device& dev) {
    auto pv = new virtio_console<virtio_mmio_device>(dev);
    if (!pv) {
        print("new failed\n");
        return false;
    }

    object_ref ref{*pv};

    auto& v = *pv;

    if (!v.dev.initialized)
        return false;

    v.dev.finalize();

    // FIXME - make sure devices are numbered consistently

    {
        char s[10];
        span sp(s, sizeof(s));

        sp = format(sp, "hvc{}", virtio_console_num.fetch_add(1));

        name_object(v, string_view(s, (size_t)(sp.data() - s)));
    }

    return true;
}
