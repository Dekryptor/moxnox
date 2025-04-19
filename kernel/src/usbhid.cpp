module;

#include "kernel.h"
#include <span>

export module usbhid;

import print;
import xhci;
import usb;

using namespace std;

static bool found_usb_hid_device(usb_device& d, uint8_t bInterfaceNumber);

static usb_recognizer mod_usb_interfaces = {
    {}, usb_class_code::hid, found_usb_hid_device
};

static void init_usbhid() {
    insert_tail_list(usb_interfaces, mod_usb_interfaces.le);
}

static module_init module_init_usbhid(init_usbhid);

static bool found_usb_hid_interface(usb_device& d, span<const uint8_t> buf) {
    print("FIXME - found_usb_hid_interface\n"); // FIXME

    auto& i = *(usb_interface_descriptor*)buf.data();

    // FIXME

    return false;
}

static bool found_usb_hid_device(usb_device& d, uint8_t bInterfaceNumber) {
    // FIXME - should be parsing HID descriptors

    {
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
    }

    return false;
}
