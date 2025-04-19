module;

#include "kernel.h"

export module usbhid;

import print;
import xhci;

static bool found_usb_hid_device(usb_device& d, uint8_t bInterfaceNumber);

static usb_recognizer mod_usb_interfaces = {
    {}, usb_class_code::hid, found_usb_hid_device
};

static void init_usbhid() {
    insert_tail_list(usb_interfaces, mod_usb_interfaces.le);
}

static module_init module_init_usbhid(init_usbhid);

static bool found_usb_hid_device(usb_device& d, uint8_t bInterfaceNumber) {
    print("FIXME - found_usb_hid_device ({})\n", bInterfaceNumber); // FIXME

    return false;
}
