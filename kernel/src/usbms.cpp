module;

export module usbms;

import usb;
import xhci;
import list;
import module_init;
import print;

static bool found_usb_storage_device(usb_device& d,
                                     const usb_device_config& config,
                                     const usb_interface& interface);

static usb_recognizer mod_usb_interfaces = {
    {}, usb_class_code::mass_storage, found_usb_storage_device
};

static void init_usbms() {
    insert_tail_list(usb_interfaces, mod_usb_interfaces.le);
}

static module_init module_init_usbms(init_usbms);

static bool found_usb_storage_device(usb_device& d,
                                     const usb_device_config& config,
                                     const usb_interface& interface) {
    print("FIXME - found_usb_storage_device\n");

    // FIXME

    return false;
}
