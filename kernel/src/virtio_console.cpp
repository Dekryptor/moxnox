module;

#include <span>
#include <atomic>
#include <string_view>

export module virtio_console;

import nt;
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
import console;
import handle;
import object_ref;

using namespace std;

static const unsigned int serial_buffer_pages = 1;

const uint64_t VIRTIO_CONSOLE_F_SIZE = 1 << 0;
const uint64_t VIRTIO_CONSOLE_F_MULTIPORT = 1 << 1;
const uint64_t VIRTIO_CONSOLE_F_EMERG_WRITE = 1 << 2;

const uint64_t SUPPORTED_FEATURES = VIRTIO_CONSOLE_F_EMERG_WRITE;

struct virtio_console_config {
    uint16_t cols;
    uint16_t rows;
    uint32_t max_nr_ports;
    uint32_t emerg_wr;
} __attribute((packed));

static_assert(sizeof(virtio_console_config) == 12);

static const uint16_t PCI_VENDOR_ID_REDHAT = 0x1af4;

void found_virtio_pci_console(pci_device& dev);

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
class virtio_console : public chardev {
public:
    virtio_console(pci_device& d, pipe& recv_pipe);
    virtio_console(T& t, pipe& recv_pipe);

    ~virtio_console() {
        recv_pipe.release();
        delete &dev;
    }

    nt_status read(span<uint8_t> s, uint64_t& read_bytes) override {
        return recv_pipe.read(s, read_bytes);
    }

    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override;

    sync_object& get_sync_object() override {
        return recv_pipe;
    }

    nt_status write(string_view sv, uint64_t& written_bytes) {
        return write(span((uint8_t*)sv.data(), sv.length()), written_bytes);
    }

    void recv_thread();

    T& dev;
    pipe& recv_pipe;

private:
    virt_queue* recv_queue = nullptr;
    virt_queue* send_queue = nullptr;
};

static atomic<uint32_t> virtio_console_num = 0;

template<>
virtio_console<virtio_pci_device>::virtio_console(pci_device& d, pipe& recv_pipe) : dev(*new virtio_pci_device(d, SUPPORTED_FEATURES, 2)), recv_pipe(recv_pipe) {
    recv_pipe.acquire();

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

    add_kernel_thread([](void* ctx) {
        ((virtio_console<virtio_pci_device>*)ctx)->recv_thread();
    }, this);
}

template<>
virtio_console<virtio_mmio_device>::virtio_console(virtio_mmio_device& d, pipe& recv_pipe) : dev(d), recv_pipe(recv_pipe) {
    recv_pipe.acquire();

    for (auto le = dev.virtqueues.next; le != &dev.virtqueues; le = le->next) {
        auto& vq = containing_record<virt_queue>(le);

        if (vq.num == 0)
            recv_queue = &vq;
        else if (vq.num == 1)
            send_queue = &vq;
    }

    add_kernel_thread([](void* ctx) {
        ((virtio_console<virtio_mmio_device>*)ctx)->recv_thread();
    }, this);
}

template<typename T>
void virtio_console<T>::recv_thread() {
    // FIXME - die gracefully if device ever goes away

    auto buf_size = PAGE_SIZE;
    auto buf = (uint8_t*)malloc(buf_size);
    if (!buf) {
        print("malloc failed\n");
        return;
    }

    while (true) {
        uint64_t buf_read_bytes, written_bytes;

        auto status = recv_queue->read(span(buf, buf_size), buf_read_bytes);

        if (!success(status)) {
            print("recv_queue->read returned {}\n", status);
            continue;
        }

        if (buf_read_bytes == 0)
            continue;

        auto read_buf = span(buf, buf_read_bytes);

        while (true) {
            status = recv_pipe.write(read_buf, written_bytes);
            if (!success(status)) {
                print("recv_pipe.write returned {}\n", status);
                break;
            }

            if (written_bytes == read_buf.size())
                break;

            read_buf = span(read_buf.data() + written_bytes,
                            read_buf.size() - written_bytes);
        }
    }
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

static object_ref create_virtio_console_in_pipe(unsigned int buffer_pages) {
    auto pptr = new pipe(buffer_pages);
    if (!pptr) {
        print("new failed\n");
        return {};
    }

    auto& p = *pptr;

    return p;
}

void found_virtio_pci_console(pci_device& dev) {
    auto recv_handle = create_virtio_console_in_pipe(serial_buffer_pages);
    if (!recv_handle) {
        print("create_virtio_console_in_pipe failed\n");
        return;
    }

    auto& recv_pipe = static_cast<pipe&>(recv_handle.get());

    auto v = new virtio_console<virtio_pci_device>(dev, recv_pipe);
    if (!v) {
        print("new failed\n");
        return;
    }

    object_ref ref{*v};

    if (!v->dev.initialized)
        return;

    v->dev.finalize();

    // FIXME - make sure devices are numbered consistently

    auto num = virtio_console_num.fetch_add(1);

    {
        char s[10];
        span sp(s, sizeof(s));

        sp = format(sp, "hvc{}", num);

        auto status = name_object(*v, string_view(s, (size_t)(sp.data() - s)));
        if (!success(status))
            print("name_object returned {}\n", status);
    }

    if (num == 0 && !com1_pipe_handle)
        com1_pipe_handle.swap(ref);
}

static bool found_virtio_mmio_console(virtio_mmio_device& dev) {
    auto recv_handle = create_virtio_console_in_pipe(serial_buffer_pages);
    if (!recv_handle) {
        print("create_virtio_console_in_pipe failed\n");
        return false;
    }

    auto& recv_pipe = static_cast<pipe&>(recv_handle.get());

    auto pv = new virtio_console<virtio_mmio_device>(dev, recv_pipe);
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

    auto num = virtio_console_num.fetch_add(1);

    {
        char s[10];
        span sp(s, sizeof(s));

        sp = format(sp, "hvc{}", num);

        auto status = name_object(v, string_view(s, (size_t)(sp.data() - s)));
        if (!success(status)) {
            print("name_object returned {}\n", status);
            return false;
        }
    }

    if (num == 0 && !com1_pipe_handle)
        com1_pipe_handle.swap(ref);

    return true;
}
