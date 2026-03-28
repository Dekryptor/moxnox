#include <span>
#include <atomic>
#include <string_view>
#include <stdint.h>

import nt;
import print;
import format;
import pci;
import virtio;
import list;
import module_init;
import thread;
import handle;
import object_ref;
import keyboard;

using namespace std;

static const uint64_t SUPPORTED_FEATURES = 0;

static const uint16_t PCI_VENDOR_ID_REDHAT = 0x1af4;

static void found_virtio_pci_input(pci_device& dev);

static pci_recognizer mod_pci_ids[] = {
    { {}, PCI_VENDOR_ID_REDHAT, 0x1052, true, found_virtio_pci_input },
};

static bool found_virtio_mmio_input(virtio_mmio_device& v);

static virtio_mmio_recognizer mod_virtio_mmio_types = {
    {}, 18, SUPPORTED_FEATURES, 2, found_virtio_mmio_input
};

static void init_virtio_pci() {
    for (unsigned int i = 0; i < sizeof(mod_pci_ids) / sizeof(*mod_pci_ids); i++) {
        insert_tail_list(pci_ids, mod_pci_ids[i].le);
    }

    insert_tail_list(virtio_mmio_types, mod_virtio_mmio_types.le);
}

static module_init module_init_virtio_input(init_virtio_pci);

template<virtio_device T>
class virtio_input : public evdev_device {
public:
    virtio_input(T& t);

    ~virtio_input() {
        delete &dev;
    }

    void recv_thread();

    T& dev;

private:
    virt_queue* event_queue = nullptr;
    virt_queue* status_queue = nullptr;
};

static atomic<uint32_t> virtio_input_num = 0;

template<virtio_device T>
virtio_input<T>::virtio_input(T& dev) : dev(dev) {
    for (auto le = dev.virtqueues.next; le != &dev.virtqueues; le = le->next) {
        auto& vq = containing_record<virt_queue>(le);

        if (vq.num == 0)
            event_queue = &vq;
        else if (vq.num == 1)
            status_queue = &vq;
    }

    this->acquire();

    auto t = add_kernel_thread([](void* ctx) {
        auto& dev = *(virtio_input<T>*)ctx;

        dev.recv_thread();
        dev.release();
    }, this);

    if (!t) {
        print("add_kernel_thread failed\n");
        this->release();
        return;
    }
}

template<virtio_device T>
void virtio_input<T>::recv_thread() {
    // FIXME - die gracefully if device ever goes away

    print("FIXME - virtio_input::recv_thread\n");
}

static void found_virtio_pci_input(pci_device& dev) {
    print("FIXME - found_virtio_pci_input\n");

    auto d = new virtio_pci_device(dev, SUPPORTED_FEATURES, 2);
    if (!d) {
        print("new failed\n");
        return;
    }

    auto v = new virtio_input(*d);
    if (!v) {
        print("new failed\n");
        delete d;
        return;
    }

    object_ref ref{*v};

    if (!v->dev.initialized)
        return;

    v->dev.finalize();

    // FIXME - make sure devices are numbered consistently

    auto num = virtio_input_num.fetch_add(1);

    {
        char s[20];
        span sp(s, sizeof(s));

        sp = format(sp, "virtioinput{}", num);

        auto status = name_object(*v, string_view(s, (size_t)(sp.data() - s)));
        if (!success(status))
            print("name_object returned {}\n", status);
    }
}

static bool found_virtio_mmio_input(virtio_mmio_device& dev) {
    print("FIXME - found_virtio_mmio_input\n");

    auto pv = new virtio_input(dev);
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

    auto num = virtio_input_num.fetch_add(1);

    {
        char s[20];
        span sp(s, sizeof(s));

        sp = format(sp, "virtioinput{}", num);

        auto status = name_object(v, string_view(s, (size_t)(sp.data() - s)));
        if (!success(status)) {
            print("name_object returned {}\n", status);
            return false;
        }
    }

    return true;
}
