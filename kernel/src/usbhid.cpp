module;

#include "kernel.h"
#include <span>

export module usbhid;

import print;
import xhci;
import usb;

using namespace std;

class usb_keyboard {
public:
    usb_keyboard(usb_device& d, xhci_queue& queue);
    ~usb_keyboard();

private:
    void recv_thread();

    usb_device& d;
    xhci_queue& queue;
};

static bool found_usb_hid_device(usb_device& d, uint8_t bInterfaceNumber);

static usb_recognizer mod_usb_interfaces = {
    {}, usb_class_code::hid, found_usb_hid_device
};

static void init_usbhid() {
    insert_tail_list(usb_interfaces, mod_usb_interfaces.le);
}

static module_init module_init_usbhid(init_usbhid);

usb_keyboard::usb_keyboard(usb_device& d, xhci_queue& queue) : d(d), queue(queue) {
    d.acquire();

    add_kernel_thread([](void* ctx) {
        ((usb_keyboard*)ctx)->recv_thread();
    }, this);
}

usb_keyboard::~usb_keyboard() {
    d.release();
}

void usb_keyboard::recv_thread() {
    auto buf = (uint8_t*)malloc(8);
    if (!buf) {
        print("malloc failed\n");
        return;
    }

    print("USB keyboard thread\n");

    while (true) {
        if (!queue.normal_transfer(span(buf, 8))) {
            print("xhci_queue::normal transfer failed\n");
            free(buf);
            return;
        }

        // FIXME
        print("{:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}\n",
              buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
    }

    free(buf);
}

static bool found_usb_keyboard(usb_device& d, span<const uint8_t> buf) {
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
                    print("found_usb_keyboard: multiple potential endpoints found\n");
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
        print("found_usb_keyboard: no potential endpoint found\n");
        return false;
    }

    print("found_usb_keyboard: FIXME - set up endpoint\n");

    auto queue = d.add_endpoint(*ep_desc);

    if (!queue) {
        print("usb_device::add_endpoint failed\n");
        return false;
    }

    auto kbd = new usb_keyboard(d, *queue);
    if (!kbd) {
        print("new failed\n");
        return false;
    }

    return true;
}

static bool found_usb_hid_interface(usb_device& d, span<const uint8_t> buf) {
    auto& i = *(usb_interface_descriptor*)buf.data();

    // FIXME - should be parsing HID descriptors

    // FIXME - reject anything for now that doesn't have a keyboard boot interface
    if (i.bInterfaceSubClass != 1 || i.bInterfaceProtocol != 1)
        return false;

    return found_usb_keyboard(d, buf);
}

static bool found_usb_hid_device(usb_device& d, uint8_t bInterfaceNumber) {
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
                    return found_usb_hid_interface(d, buf);

                break;
            }

            default:
                break;
        }

        buf = buf.subspan(len);
    }

    return false;
}
