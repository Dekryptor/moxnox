module;

#include <stdint.h>

export module usbms;

import usb;
import xhci;
import list;
import module_init;
import print;

enum class usb_subclass_mass_storage : uint8_t {
    rbc = 1,
    mmc5 = 2,
    ufi = 4,
    scsi_transparent_command_set = 6,
    lsdfs = 7,
    ieee1667 = 8,
};

enum class usb_protocol_mass_storage : uint8_t {
    cbi_no_interrupt = 0x00,
    cbi_with_interrupt = 0x01,
    bbb = 0x50,
    uas = 0x62,
};

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

static bool found_usb_mass_storage(usb_device& d,
                                   const usb_device_config& config,
                                   const usb_interface& interface) {
    print("FIXME - found_usb_mass_storage\n");

    // FIXME

    return false;
}

static bool found_usb_storage_device(usb_device& d,
                                     const usb_device_config& config,
                                     const usb_interface& interface) {
    auto subclass = (usb_subclass_mass_storage)interface.desc.bInterfaceSubClass;

    if (subclass != usb_subclass_mass_storage::scsi_transparent_command_set)
        return false;

    auto proto = (usb_protocol_mass_storage)interface.desc.bInterfaceProtocol;

    if (proto != usb_protocol_mass_storage::bbb)
        return false;

    return found_usb_mass_storage(d, config, interface);
}
