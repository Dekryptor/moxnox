module;

#include <stdint.h>
#include <span>

export module usbms;

import usb;
import xhci;
import list;
import module_init;
import print;
import crt;

using namespace std;

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

enum class usbms_request : uint8_t {
    GET_MAX_LUN = 0xfe,
    BULK_ONLY_MASS_STORAGE_RESET = 0xff,
};

enum class scsi_command : uint8_t {
    TEST_UNIT_READY = 0x00,
};

struct scsi_test_unit_ready {
    enum scsi_command cmd;
    uint8_t reserved[4];
    uint8_t control;
} __attribute((packed));

struct usbms_cbw {
    uint32_t dCBWSignature;
    uint32_t dCBWTag;
    uint32_t dCBWDataTransferLength;
    uint8_t bmCBWFlags;
    uint8_t bCBWLUN;
    uint8_t bCBWCBLength;
    uint8_t msg[0x10];
} __attribute((packed));

static_assert(sizeof(usbms_cbw) == 0x1f);

static const uint32_t USBMS_CBW_SIG = 0x43425355;

enum class csw_status : uint8_t {
    success = 0,
    failed = 1,
    phase_error = 2,
};

struct usbms_csw {
    uint32_t dCSWSignature;
    uint32_t dCSWTag;
    uint32_t dCSWDataResidue;
    csw_status bCSWStatus;
} __attribute((packed));

static_assert(sizeof(usbms_csw) == 0xd);

static const uint32_t USBMS_CSW_SIG = 0x53425355;

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

static bool usbms_get_max_lun(usb_device& d, uint8_t& max_lun,
                              uint16_t interface) {
    return d.ep0_queue.control_transfer(span<uint8_t>(&max_lun, 1),
                                        USB_REQUEST_TYPE_DEVICE_TO_HOST | USB_REQUEST_TYPE_CLASS | USB_REQUEST_TYPE_RECIPIENT_INTERFACE,
                                        (enum usb_request)usbms_request::GET_MAX_LUN,
                                        0, interface);
}

static bool test_unit_ready(xhci_queue& in_queue, xhci_queue& out_queue,
                            uint8_t lun) {
    usbms_cbw cbw;
    usbms_csw csw;

    // FIXME - handle stalls

    // prepare command

    memset(&cbw, 0, sizeof(cbw));

    cbw.dCBWSignature = USBMS_CBW_SIG;
    cbw.dCBWTag = 0; // FIXME
    cbw.dCBWDataTransferLength = 0; // no data
    cbw.bmCBWFlags = 0;
    cbw.bCBWLUN = lun;
    cbw.bCBWCBLength = sizeof(cbw.msg);

    auto& msg = *(scsi_test_unit_ready*)cbw.msg;

    msg.cmd = scsi_command::TEST_UNIT_READY;

    // send command

    auto sent_bytes = out_queue.normal_transfer(span((uint8_t*)&cbw, sizeof(cbw)));

    if (sent_bytes == 0) {
        print("test_unit_ready: xhci_queue::normal_transfer failed for OUT endpoint\n");
        return false;
    } else if (sent_bytes != sizeof(cbw)) {
        print("test_unit_ready: xhci_queue::normal_transfer sent {} bytes, expected {}\n",
              sent_bytes, sizeof(cbw));
        return false;
    }

    // wait for reply

    auto recv_bytes = in_queue.normal_transfer(span((uint8_t*)&csw, sizeof(csw)));

    if (recv_bytes == 0) {
        print("test_unit_ready: xhci_queue::normal_transfer failed for IN endpoint\n");
        return false;
    } else if (recv_bytes != sizeof(csw)) {
        print("test_unit_ready: xhci_queue::normal_transfer received {} bytes, expected {}\n",
              recv_bytes, sizeof(csw));
        return false;
    }

    if (csw.dCSWSignature != USBMS_CSW_SIG) {
        print("test_unit_ready: response had invalid signature\n");
        return false;
    }

    if (csw.dCSWTag != cbw.dCBWTag) {
        print("test_unit_ready: response had invalid tag\n");
        return false;
    }

    // FIXME - for data messages, check dCSWDataResidue

    print("received reply (bCSWStatus = {})\n", (uint8_t)csw.bCSWStatus);

    switch (csw.bCSWStatus) {
        case csw_status::success:
            return true;

        case csw_status::failed:
            print("test_unit_ready: command failed\n");
            return false;

        case csw_status::phase_error:
            print("test_unit_ready: phase error\n");
            return false;

        default:
            print("test_unit_ready: unrecognized error status {:02x}\n",
                  (uint8_t)csw.bCSWStatus);
            return false;
    }
}

static void init_lun(usb_device& d, xhci_queue& in_queue,
                     xhci_queue& out_queue, uint8_t lun) {
    // FIXME - mutex?

    // FIXME - repeat if necessary?
    if (!test_unit_ready(in_queue, out_queue, lun))
        return;

    // FIXME - INQUIRY (0x12)
    // FIXME - PREVENT ALLOW MEDIUM REMOVAL?
    // FIXME - READ CAPACITY (0x25)
    // FIXME - actual I/O (READ [0x28] and WRITE [0x2a])

    // FIXME - add block device
    // FIXME - print found message with name(?), block size, and size
    // FIXME - scan for partitions

    // FIXME - handle removals
}

static bool found_usb_mass_storage(usb_device& d,
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

    auto in_queue = d.configure_endpoint(in_ep->desc);
    if (!in_queue) {
        print("usb_device::configure_endpoint failed for IN endpoint\n");
        return false;
    }

    auto out_queue = d.configure_endpoint(out_ep->desc);
    if (!out_queue) {
        print("usb_device::configure_endpoint failed for OUT endpoint\n");
        return false;
    }

    uint8_t max_lun;

    if (!usbms_get_max_lun(d, max_lun, interface.desc.bInterfaceNumber)) {
        print("found_usb_mass_storage: GET MAX LUN failed\n");
        return false;
    }

    if (max_lun > 15) {
        print("found_usb_mass_storage: max LUN was {}, putting down to 15\n",
              max_lun);
        max_lun = 15;
    }

    for (uint8_t i = 0; i <= max_lun; i++) {
        init_lun(d, *in_queue, *out_queue, i);
    }

    // FIXME - return false if all LUNs failed?

    return true;
}

static bool found_usb_storage_device(usb_device& d,
                                     const usb_device_config&,
                                     const usb_interface& interface) {
    auto subclass = (usb_subclass_mass_storage)interface.desc.bInterfaceSubClass;

    if (subclass != usb_subclass_mass_storage::scsi_transparent_command_set)
        return false;

    auto proto = (usb_protocol_mass_storage)interface.desc.bInterfaceProtocol;

    if (proto != usb_protocol_mass_storage::bbb)
        return false;

    return found_usb_mass_storage(d, interface);
}
