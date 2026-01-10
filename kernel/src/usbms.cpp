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
    usb_endpoint* in_ep = nullptr;
    usb_endpoint* out_ep = nullptr;

    print("FIXME - found_usb_mass_storage\n");

    // find endpoint numbers

    for (auto le = interface.endpoints.next; le != &interface.endpoints; le = le->next) {
        auto& e = containing_record<usb_endpoint>(le);

        // look for periodic bulk endpoints

        if (e.desc.transfer_type != USB_ENDPOINT_TRANSFER_TYPE_BULK)
            continue;

        if (e.desc.usage_type != USB_ENDPOINT_USAGE_TYPE_PERIODIC)
            continue;

        if (e.desc.direction == USB_ENDPOINT_DIRECTION_IN) {
            if (in_ep) {
                print("found_usb_mass_storage: multiple potential IN endpoints found\n");
                return false;
            }

            in_ep = &e;
        } else {
            if (out_ep) {
                print("found_usb_mass_storage: multiple potential OUT endpoints found\n");
                return false;
            }

            out_ep = &e;
        }
    }

    if (!in_ep && !out_ep) {
        print("found_usb_mass_storage: no potential endpoints found\n");
        return false;
    } else if (!in_ep) {
        print("found_usb_mass_storage: no potential IN endpoint found\n");
        return false;
    } else if (!out_ep) {
        print("found_usb_mass_storage: no potential OUT endpoint found\n");
        return false;
    }

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
