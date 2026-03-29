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
import mem;
import thread;
import handle;
import object_ref;
import keyboard;
import dma;
import evdev;

using namespace std;

static const uint64_t SUPPORTED_FEATURES = 0;

struct virtio_input_event {
    evdev_type type;
    evdev_code code;
    int32_t value;
} __attribute((packed));

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
        if (kc)
            kc->unregister();

        delete &dev;
    }

    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override;

    void recv_thread();

    T& dev;
    keyboard_context* kc = nullptr;

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

    this->kc = register_keyboard(*this);
}

template<virtio_device T>
void virtio_input<T>::recv_thread() {
    // FIXME - die gracefully if device ever goes away

    static const size_t BUFFER_LENGTH = 16;

    virtio_input_event evs[BUFFER_LENGTH];
    uint8_t* bounce[BUFFER_LENGTH];
    uint8_t slot[BUFFER_LENGTH];
    uintptr_t pa[BUFFER_LENGTH];
    size_t idx = 0;

    for (size_t i = 0; i < BUFFER_LENGTH; i++) {
        pa[i] = dma_setup(span((uint8_t*)&evs[i], sizeof(evs[i])),
                          false, bounce[i]);
        if (!pa[i]) {
            print("dma_setup failed\n");

            for (size_t j = 0; j < i; j++) {
                dma_finish_read(span((uint8_t*)&evs[j], sizeof(evs[j])),
                                bounce[j]);
            }

            return;
        }

        flush_cache(span((uint8_t*)phys_address(pa[i]), sizeof(virtio_input_event)),
                    false);

        slot[i] = event_queue->async_read(pa[i], sizeof(virtio_input_event));
    }

    while (true) {
        auto len = event_queue->wait_read(slot[idx]);

        if (len >= sizeof(virtio_input_event)) {
            if (bounce[idx]) {
                flush_cache(span(bounce[idx], sizeof(virtio_input_event)), true);
                memcpy(&evs[idx], bounce[idx], sizeof(virtio_input_event));
            } else
                flush_cache(span((uint8_t*)&evs[idx], sizeof(virtio_input_event)), true);

#if 0
            print("ev: type = {}, code = {}, value = {}\n",
                  (evdev_type)evs[idx].type, (evdev_code)evs[idx].code,
                  +evs[idx].value);
#endif

            input_event ev;

            ev.type = evs[idx].type;
            ev.code = evs[idx].code;
            ev.value = evs[idx].value;

            post_event(ev);
        }

        slot[idx] = event_queue->async_read(pa[idx], sizeof(virtio_input_event));
        idx = (idx + 1) % BUFFER_LENGTH;
    }

    for (size_t i = 0; i < BUFFER_LENGTH; i++) {
        if (bounce[i])
            free(bounce[i]);
    }
}

template<virtio_device T>
nt_status virtio_input<T>::write(span<const uint8_t> s, uint64_t& written_bytes) {
    if (s.size() < sizeof(input_event))
        return nt_status::INVALID_PARAMETER;

    auto& ev = *(input_event*)s.data();

    virtio_input_event vie;

    vie.type = ev.type;
    vie.code = ev.code;
    vie.value = ev.value;

    auto status = status_queue->write(span((uint8_t*)&vie, sizeof(vie)));

    if (!success(status))
        return status;

    written_bytes = sizeof(input_event);

    return nt_status::SUCCESS;
}

static void found_virtio_pci_input(pci_device& dev) {
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
