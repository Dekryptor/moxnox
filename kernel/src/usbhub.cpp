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
    usb_hub(usb_device& d, xhci_queue& queue, usb_hub_descriptor* hub_desc);
    ~usb_hub();
    void recv_thread();

    bool initialized = false;

private:
    bool set_port_feature(enum usb_port_feature_selector feature, uint16_t index);

    usb_device& d;
    xhci_queue& queue;
    usb_hub_descriptor* hub_desc = nullptr;
};

static bool found_usb_hub_device(usb_device& d, uint8_t bInterfaceNumber);

static usb_recognizer mod_usb_interfaces = {
    {}, usb_class_code::hub, found_usb_hub_device
};

static void init_usbhub() {
    insert_tail_list(usb_interfaces, mod_usb_interfaces.le);
}

static module_init module_init_usbhub(init_usbhub);

usb_hub::usb_hub(usb_device& d, xhci_queue& queue, usb_hub_descriptor* hub_desc) : d(d), queue(queue) {
    this->hub_desc = hub_desc;

    d.acquire();

    for (uint8_t port = 1; port <= hub_desc->bNbrPorts; port++) {
        if (!set_port_feature(usb_port_feature_selector::PORT_POWER, port)) {
            print("usb_hub::set_port_feature failed\n");
            return;
        }
    }

    initialized = true;
}

usb_hub::~usb_hub() {
    d.release();

    if (hub_desc)
        free(hub_desc);
}

bool usb_hub::set_port_feature(enum usb_port_feature_selector feature,
                               uint16_t index) {
    if (!d.set_feature(USB_REQUEST_TYPE_HOST_TO_DEVICE | USB_REQUEST_TYPE_CLASS | USB_REQUEST_TYPE_RECIPIENT_OTHER,
                       (uint8_t)feature, index)) {
        print("usb_device::set_feature failed\n");
        return false;
    }

    return true;
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

static usb_hub_descriptor* get_hub_descriptor(usb_device& d) {
    // FIXME - "enhanced" hubs

    size_t size = offsetof(usb_hub_descriptor, DeviceRemovable);

    auto desc = (usb_hub_descriptor*)malloc(size);
    if (!desc) {
        print("malloc failed\n");
        return nullptr;
    }

    if (!d.get_descriptor_class(span((uint8_t*)desc, size), usb_descriptor::HUB, 0)) {
        print("found_usb_hub_interface: could not get HUB descriptor\n");
        free(desc);
        return nullptr;
    }

    if (desc->bDescLength > size) {
        size = desc->bDescLength;

        free(desc);

        desc = (usb_hub_descriptor*)malloc(size);
        if (!desc) {
            print("malloc failed\n");
            return nullptr;
        }

        if (!d.get_descriptor_class(span((uint8_t*)desc, size), usb_descriptor::HUB, 0)) {
            print("found_usb_hub_interface: could not get HUB descriptor\n");
            free(desc);
            return nullptr;
        }
    }

    print_usb_config(span((uint8_t*)desc, size));

    return desc;
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

    auto hub_desc = get_hub_descriptor(d);
    if (!hub_desc) {
        print("get_hub_descriptor failed\n");
        return false;
    }

    auto hub = new usb_hub(d, *queue, hub_desc);
    if (!hub) {
        print("new failed\n");
        free(hub_desc);
        return false;
    }

    if (!hub->initialized) {
        print("usb_hub::usb_hub failed\n");
        delete hub;
        return false;
    }

    add_kernel_thread([](void* ctx) {
        ((usb_hub*)ctx)->recv_thread();
    }, hub);

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
