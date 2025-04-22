module;

#include "kernel.h"
#include <span>

export module usbhub;

import print;
import xhci;
import usb;

using namespace std;

class usb_hub {
public:
    usb_hub(usb_device& d, xhci_queue& queue);
    ~usb_hub();

private:
    void recv_thread();

    usb_device& d;
    xhci_queue& queue;
};

static bool found_usb_hub_device(usb_device& d, uint8_t bInterfaceNumber);

static usb_recognizer mod_usb_interfaces = {
    {}, usb_class_code::hub, found_usb_hub_device
};

static void init_usbhub() {
    insert_tail_list(usb_interfaces, mod_usb_interfaces.le);
}

static module_init module_init_usbhub(init_usbhub);

usb_hub::usb_hub(usb_device& d, xhci_queue& queue) : d(d), queue(queue) {
    d.acquire();

    add_kernel_thread([](void* ctx) {
        ((usb_hub*)ctx)->recv_thread();
    }, this);
}

usb_hub::~usb_hub() {
    d.release();
}

void usb_hub::recv_thread() {
    auto buf = (uint8_t*)malloc(2);
    if (!buf) {
        print("malloc failed\n");
        return;
    }

    print("USB hub thread\n");

    while (true) {
        if (!queue.normal_transfer(span(buf, 2))) {
            print("xhci_queue::normal transfer failed\n");
            free(buf);
            return;
        }

        // FIXME
        print("{:02x} {:02x}\n", buf[0], buf[1]);
    }

    free(buf);
}

static bool found_usb_hub_interface(usb_device& d, span<const uint8_t> buf) {
    auto& i = *(usb_interface_descriptor*)buf.data();

    usb_endpoint_descriptor* ep_desc = nullptr;

    // find endpoint number

    // skip INTERFACE
    buf = buf.subspan(i.bLength);

    while (!buf.empty()) {
        auto len = buf[0];

        if (len > buf.size() || len < 2) {
            print("buffer truncated\n");
            return false;
        }

        auto type = (enum usb_descriptor)buf[1];

        if (type == usb_descriptor::INTERFACE)
            break;

        switch (type) {
            case usb_descriptor::ENDPOINT: {
                auto& e = *(usb_endpoint_descriptor*)buf.data();

                if (len < sizeof(usb_endpoint_descriptor)) {
                    print("buffer truncated\n");
                    return false;
                }

                // look for periodic interrupt IN endpoint

                if (e.direction != USB_ENDPOINT_DIRECTION_IN)
                    break;

                if (e.transfer_type != USB_ENDPOINT_TRANSFER_TYPE_INTERRUPT)
                    continue;

                if (e.usage_type != USB_ENDPOINT_USAGE_TYPE_PERIODIC)
                    continue;

                if (ep_desc) {
                    print("found_usb_hub_interface: multiple potential endpoints found\n");
                    return false;
                }

                ep_desc = &e;

                break;
            }

            default:
                break;
        }

        buf = buf.subspan(len);
    }

    if (!ep_desc) {
        print("found_usb_hub_interface: no potential endpoint found\n");
        return false;
    }

    auto queue = d.add_endpoint(*ep_desc);

    if (!queue) {
        print("usb_device::add_endpoint failed\n");
        return false;
    }

    auto hub = new usb_hub(d, *queue);
    if (!hub) {
        print("new failed\n");
        return false;
    }

    return true;
}

static bool found_usb_hub_device(usb_device& d, uint8_t bInterfaceNumber) {
    auto buf = span((uint8_t*)d.config, d.config_size);

    while (!buf.empty()) {
        auto len = buf[0];

        if (len > buf.size() || len < 2) {
            print("buffer truncated\n");
            return false;
        }

        auto type = (enum usb_descriptor)buf[1];

        switch (type) {
            case usb_descriptor::INTERFACE: {
                auto& i = *(usb_interface_descriptor*)buf.data();

                if (len < sizeof(usb_interface_descriptor)) {
                    print("buffer truncated\n");
                    return false;
                }

                if (i.bInterfaceNumber == bInterfaceNumber)
                    return found_usb_hub_interface(d, buf);

                break;
            }

            default:
                break;
        }

        buf = buf.subspan(len);
    }

    return false;
}
