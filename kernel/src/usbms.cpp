module;

#include <stdint.h>
#include <span>
#include <bit>
#include <string_view>
#include <atomic>

export module usbms;

import usb;
import xhci;
import list;
import module_init;
import print;
import format;
import crt;
import mem;
import nt;
import page_cache;
import volume;
import object_ref;
import handle;
import timer;
import config;

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
    INQUIRY = 0x12,
    READ_CAPACITY10 = 0x25,
    READ10 = 0x28,
    READ_CAPACITY16 = 0x9e,
};

struct scsi_test_unit_ready {
    enum scsi_command cmd;
    uint8_t reserved[4];
    uint8_t control;
} __attribute((packed));

static_assert(sizeof(scsi_test_unit_ready) == 6);

struct scsi_inquiry_cmd {
    enum scsi_command cmd;
    uint8_t evpd : 1;
    uint8_t cmddt : 1;
    uint8_t : 6;
    uint8_t page_code;
    uint16_t allocation_length;
    uint8_t control;
} __attribute((packed));

static_assert(sizeof(scsi_inquiry_cmd) == 6);

struct scsi_read_capacity10 {
    enum scsi_command cmd;
    uint8_t reserved;
    uint32_t lba;
    uint16_t reserved2;
    uint8_t reserved3;
    uint8_t control;
} __attribute((packed));

static_assert(sizeof(scsi_read_capacity10) == 10);

struct scsi_read_capacity10_resp {
    uint32_t lba;
    uint32_t block_length;
} __attribute((packed));

static_assert(sizeof(scsi_read_capacity10_resp) == 8);

struct scsi_read_capacity16 {
    enum scsi_command cmd;
    uint8_t service_action : 5;
    uint8_t : 3;
    uint64_t lba;
    uint32_t allocation_length;
    uint8_t reserved;
    uint8_t control;
} __attribute((packed));

static_assert(sizeof(scsi_read_capacity16) == 16);

struct scsi_read_capacity16_resp {
    uint64_t lba;
    uint32_t block_length;
    uint8_t prot_en : 1;
    uint8_t p_type : 3;
    uint8_t rc_basis : 2;
    uint8_t : 2;
    uint8_t logical_blocks_per_physical_block_exponent : 4;
    uint8_t p_i_exponent : 4;
    uint16_t lowest_aligned_lba : 14;
    uint16_t lbprz : 1;
    uint16_t lbpme : 1;
    uint8_t reserved[16];
} __attribute((packed));

static_assert(sizeof(scsi_read_capacity16_resp) == 32);

struct scsi_read10 {
    enum scsi_command cmd;
    uint8_t : 2;
    uint8_t rarc : 1;
    uint8_t fua : 1;
    uint8_t dpo : 1;
    uint8_t rdprotect : 3;
    uint32_t lba;
    uint8_t group_number : 5;
    uint8_t : 3;
    uint16_t transfer_length;
    uint8_t control;
} __attribute((packed));

static_assert(sizeof(scsi_read10) == 10);

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

struct scsi_inquiry {
    uint8_t peripheral_device_type : 5;
    uint8_t peripheral_qualifier : 3;
    uint8_t : 7;
    uint8_t rmb : 1;
    uint8_t version;
    uint8_t response_data_format : 4;
    uint8_t hisup : 1;
    uint8_t normaca : 1;
    uint8_t : 2;
    uint8_t additional_length;
    uint8_t protect : 1;
    uint8_t : 2;
    uint8_t third_party_copy : 1;
    uint8_t tpgs : 2;
    uint8_t acc : 1;
    uint8_t sccs : 1;
    uint8_t : 4;
    uint8_t multip : 1;
    uint8_t vs1 : 1;
    uint8_t encserv : 1;
    uint8_t : 1;
    uint8_t vs2 : 1;
    uint8_t cmdque : 1;
    uint8_t : 6;
    char vendor_identification[8];
    char product_identification[16];
    char product_revision_level[4];
    char drive_serial_number[8];
    uint8_t vendor_unique[12];
} __attribute((packed));

static_assert(sizeof(scsi_inquiry) == 0x38);

class usbms_volume : public volume {
public:
    usbms_volume(usb_device& d, xhci_queue& in_queue,
                 xhci_queue& out_queue, uint8_t lun);

    virtual ~usbms_volume() override {
        d.release();
    }

    void release() override {
        if (--refcount == 0)
            delete this;
    }

    nt_status flush() override;
    nt_status dio_read(uint64_t page_num, uint64_t num_pages,
                       pfn_iterator& it, uint64_t& read_pages) override;
    nt_status dio_write(uint64_t page_num, uint64_t num_pages,
                        pfn_iterator& it, uint64_t& written_pages,
                        bool fua) override;

    bool initialized = false;
    unsigned int dev_num;
    char model_number_buf[16];
    string_view model_number;

private:
    bool test_unit_ready();
    bool inquiry();
    bool read_capacity10();
    bool read_capacity16();

    usb_device& d;
    xhci_queue& in_queue;
    xhci_queue& out_queue;
    uint8_t lun;
    bool big = false;
};

static atomic<unsigned int> num_usbms_devices;

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

static bool send_scsi_msg_no_data(xhci_queue& in_queue, xhci_queue& out_queue,
                                  uint8_t lun, span<uint8_t> msg) {
    usbms_cbw cbw;
    usbms_csw csw;

    // FIXME - mutex

    // FIXME - translate into nt_status?
    // FIXME - handle stalls

    // prepare command

    memset(&cbw, 0, sizeof(cbw));

    cbw.dCBWSignature = USBMS_CBW_SIG;
    cbw.dCBWTag = 0; // FIXME
    cbw.dCBWDataTransferLength = 0;
    cbw.bmCBWFlags = 0;
    cbw.bCBWLUN = lun;
    cbw.bCBWCBLength = sizeof(cbw.msg);

    memcpy(&cbw.msg, msg.data(), msg.size());

    // send command

    auto sent_bytes = out_queue.normal_transfer(span((uint8_t*)&cbw, sizeof(cbw)));

    if (sent_bytes == 0) {
        print("send_scsi_msg_no_data: xhci_queue::normal_transfer failed for OUT endpoint\n");
        return false;
    } else if (sent_bytes != sizeof(cbw)) {
        print("send_scsi_msg_no_data: xhci_queue::normal_transfer sent {} bytes, expected {}\n",
              sent_bytes, sizeof(cbw));
        return false;
    }

    // wait for reply

    auto recv_bytes = in_queue.normal_transfer(span((uint8_t*)&csw, sizeof(csw)));

    if (recv_bytes == 0) {
        print("send_scsi_msg_no_data: xhci_queue::normal_transfer failed for IN endpoint\n");
        return false;
    } else if (recv_bytes != sizeof(csw)) {
        print("send_scsi_msg_no_data: xhci_queue::normal_transfer received {} bytes, expected {}\n",
              recv_bytes, sizeof(csw));
        return false;
    }

    if (csw.dCSWSignature != USBMS_CSW_SIG) {
        print("send_scsi_msg_no_data: response had invalid signature ({:08x}, expected {:08x})\n",
              csw.dCSWSignature, USBMS_CSW_SIG);
        return false;
    }

    if (csw.dCSWTag != cbw.dCBWTag) {
        print("send_scsi_msg_no_data: response had invalid tag ({:02x}, expected {:02x})\n",
              csw.dCSWTag, cbw.dCBWTag);
        return false;
    }

    switch (csw.bCSWStatus) {
        case csw_status::success:
            return true;

        case csw_status::failed:
            print("send_scsi_msg: command failed\n");
            return false;

        case csw_status::phase_error:
            print("send_scsi_msg: phase error\n");
            return false;

        default:
            print("send_scsi_msg: unrecognized error status {:02x}\n",
                  (uint8_t)csw.bCSWStatus);
            return false;
    }
}

static size_t send_scsi_msg_in(xhci_queue& in_queue, xhci_queue& out_queue,
                               uint8_t lun, span<uint8_t> msg,
                               span<uint8_t> buf) {
    usbms_cbw cbw;
    usbms_csw csw;

    // FIXME - mutex

    // FIXME - translate into nt_status?
    // FIXME - handle stalls

    // prepare command

    memset(&cbw, 0, sizeof(cbw));

    cbw.dCBWSignature = USBMS_CBW_SIG;
    cbw.dCBWTag = 0; // FIXME
    cbw.dCBWDataTransferLength = (uint32_t)buf.size();
    cbw.bmCBWFlags = 0x80; // in
    cbw.bCBWLUN = lun;
    cbw.bCBWCBLength = sizeof(cbw.msg);

    memcpy(&cbw.msg, msg.data(), msg.size());

    // send command

    auto sent_bytes = out_queue.normal_transfer(span((uint8_t*)&cbw, sizeof(cbw)));

    if (sent_bytes == 0) {
        print("send_scsi_msg_in: xhci_queue::normal_transfer failed for OUT endpoint\n");
        return 0;
    } else if (sent_bytes != sizeof(cbw)) {
        print("send_scsi_msg_in: xhci_queue::normal_transfer sent {} bytes, expected {}\n",
              sent_bytes, sizeof(cbw));
        return 0;
    }

    // wait for data

    auto recv_bytes = in_queue.normal_transfer(span(buf.data(), buf.size()));

    if (recv_bytes == 0) {
        print("send_scsi_msg_in: xhci_queue::normal_transfer failed for IN endpoint\n");
        return 0;
    } else if (recv_bytes != buf.size()) {
        print("send_scsi_msg_in: xhci_queue::normal_transfer received {} bytes, expected {}\n",
              recv_bytes, buf.size());
        return 0;
    }

    recv_bytes = in_queue.normal_transfer(span((uint8_t*)&csw, sizeof(csw)));

    if (recv_bytes == 0) {
        print("send_scsi_msg_in: xhci_queue::normal_transfer failed for IN endpoint\n");
        return 0;
    } else if (recv_bytes != sizeof(csw)) {
        print("send_scsi_msg_in: xhci_queue::normal_transfer received {} bytes, expected {}\n",
              recv_bytes, sizeof(csw));
        return 0;
    }

    // wait for status

    if (csw.dCSWSignature != USBMS_CSW_SIG) {
        print("send_scsi_msg_in: response had invalid signature ({:08x}, expected {:08x})\n",
              csw.dCSWSignature, USBMS_CSW_SIG);
        return 0;
    }

    if (csw.dCSWTag != cbw.dCBWTag) {
        print("send_scsi_msg_in: response had invalid tag ({:02x}, expected {:02x})\n",
              csw.dCSWTag, cbw.dCBWTag);
        return 0;
    }

    switch (csw.bCSWStatus) {
        case csw_status::success:
            return buf.size() - csw.dCSWDataResidue;

        case csw_status::failed:
            print("send_scsi_msg_in: command failed\n");
            return 0;

        case csw_status::phase_error:
            print("send_scsi_msg_in: phase error\n");
            return 0;

        default:
            print("send_scsi_msg_in: unrecognized error status {:02x}\n",
                  (uint8_t)csw.bCSWStatus);
            return 0;
    }
}

bool usbms_volume::test_unit_ready() {
    scsi_test_unit_ready msg;

    memset(&msg, 0, sizeof(msg));
    msg.cmd = scsi_command::TEST_UNIT_READY;

    return send_scsi_msg_no_data(in_queue, out_queue, lun,
                                 span((uint8_t*)&msg, sizeof(msg)));
}

bool usbms_volume::inquiry() {
    scsi_inquiry_cmd msg;
    scsi_inquiry buf;

    static const size_t MIN_INQUIRY_SIZE = 36;

    memset(&msg, 0, sizeof(msg));
    msg.cmd = scsi_command::INQUIRY;
    msg.allocation_length = byteswap((uint16_t)sizeof(buf));

    auto recv_bytes = send_scsi_msg_in(in_queue, out_queue, lun,
                                       span((uint8_t*)&msg, sizeof(msg)),
                                       span((uint8_t*)&buf, sizeof(buf)));
    if (recv_bytes == 0)
        return false;

    if (recv_bytes < MIN_INQUIRY_SIZE) {
        print("usbms_volume::inquiry: received {} bytes, expected at least {}\n",
              recv_bytes, MIN_INQUIRY_SIZE);
        return false;
    }

    memcpy(model_number_buf, buf.product_identification, sizeof(model_number_buf));

    auto vendor = string_view(buf.vendor_identification, sizeof(buf.vendor_identification));

    model_number = string_view(model_number_buf, sizeof(model_number_buf));

    while (!vendor.empty() && vendor.back() == ' ') {
        vendor = string_view(vendor.data(), vendor.size() - 1);
    }

    while (!model_number.empty() && model_number.back() == ' ') {
        model_number = string_view(model_number.data(), model_number.size() - 1);
    }

    return true;
}

bool usbms_volume::read_capacity10() {
    scsi_read_capacity10 msg;
    scsi_read_capacity10_resp buf;

    memset(&msg, 0, sizeof(msg));
    msg.cmd = scsi_command::READ_CAPACITY10;

    auto recv_bytes = send_scsi_msg_in(in_queue, out_queue, lun,
                                       span((uint8_t*)&msg, sizeof(msg)),
                                       span((uint8_t*)&buf, sizeof(buf)));

    if (recv_bytes != sizeof(buf)) {
        print("usbms_volume::read_capacity10: received {} bytes, expected {}\n",
              recv_bytes, sizeof(buf));
        return false;
    }

    sector_size = byteswap(buf.block_length);
    size = ((uint64_t)byteswap(buf.lba) + 1) * sector_size;

    return true;
}

bool usbms_volume::read_capacity16() {
    scsi_read_capacity16 msg;
    scsi_read_capacity16_resp buf;

    memset(&msg, 0, sizeof(msg));
    msg.cmd = scsi_command::READ_CAPACITY16;
    msg.allocation_length = byteswap((uint16_t)sizeof(buf));

    auto recv_bytes = send_scsi_msg_in(in_queue, out_queue, lun,
                                       span((uint8_t*)&msg, sizeof(msg)),
                                       span((uint8_t*)&buf, sizeof(buf)));

    if (recv_bytes != sizeof(buf)) {
        print("usbms_volume::read_capacity16: received {} bytes, expected {}\n",
              recv_bytes, sizeof(buf));
        return false;
    }

    sector_size = byteswap(buf.block_length);
    size = (byteswap(buf.lba) + 1) * sector_size;
    big = true;

    return true;
}

usbms_volume::usbms_volume(usb_device& d, xhci_queue& in_queue,
                           xhci_queue& out_queue, uint8_t lun) :
                           d(d), in_queue(in_queue), out_queue(out_queue), lun(lun) {
    d.acquire();

    // FIXME - mutex?

    bool test_unit_ready_done = false;

    for (unsigned int i = 0; i < 20; i++) {
        if (test_unit_ready()) {
            test_unit_ready_done = true;
            break;
        }

        msleep(50);
    }

    if (!test_unit_ready_done) {
        print("usbms_volume::usbms_volume: TEST_UNIT_READY failed\n");
        return;
    }

    if (!inquiry()) {
        print("usbms_volume::usbms_volume: INQUIRY failed\n");
        return;
    }

    // FIXME - PREVENT ALLOW MEDIUM REMOVAL?

    if (!read_capacity10()) {
        print("usbms_volume::usbms_volume: READ_CAPACITY10 failed\n");
        return;
    }

    if ((size / sector_size) - 1 == 0xffffffff) {
        if (!read_capacity16()) {
            print("usbms_volume::usbms_volume: READ_CAPACITY16 failed, capping to {}\n",
                byte_size(size));
        }
    }

    initialized = true;
}

nt_status usbms_volume::flush() {
    // FIXME - SYNCHRONIZE CACHE (0x35/0x91)?

    return nt_status::SUCCESS;
}

nt_status usbms_volume::dio_read(uint64_t page_num, uint64_t num_pages,
                                 pfn_iterator& it, uint64_t& read_pages) {
    print("FIXME - usbms_volume::dio_read({:x}, {:x})\n", page_num, num_pages);

    if (num_pages == 0) {
        read_pages = 0;
        return nt_status::SUCCESS;
    }

    // FIXME - avoid copy

    // FIXME - READ16 (0x88) if big device

    auto page = allocate_folio(0);
    if (!page)
        return nt_status::INSUFFICIENT_RESOURCES;

    num_pages = 1; // FIXME

    // FIXME - limit num_pages to something sensible

    auto buf = (uint8_t*)phys_address(page);

    scsi_read10 msg;

    memset(&msg, 0, sizeof(msg));
    msg.cmd = scsi_command::READ10;
    msg.lba = byteswap((uint32_t)((page_num << PAGE_SHIFT) / sector_size));
    msg.transfer_length = byteswap((uint16_t)(num_pages * (PAGE_SIZE / sector_size)));

    auto recv_bytes = send_scsi_msg_in(in_queue, out_queue, lun,
                                       span((uint8_t*)&msg, sizeof(msg)),
                                       span(buf, PAGE_SIZE));

    if (recv_bytes != PAGE_SIZE) {
        print("usbms_volume::dio_read: received {} bytes, expected {}\n",
              recv_bytes, PAGE_SIZE);
        free_folio(page);
        return nt_status::IO_DEVICE_ERROR;
    }

    memcpy(phys_address(*it), buf, PAGE_SIZE);
    free_folio(page);

    read_pages = num_pages;

    return nt_status::SUCCESS;
}

nt_status usbms_volume::dio_write(uint64_t page_num, uint64_t num_pages,
                                  pfn_iterator& /*it*/, uint64_t& /*written_pages*/,
                                  bool fua) {
    print("FIXME - usbms_volume::dio_write({:x}, {:x}, {})\n",
          page_num, num_pages, fua ? "true" : "false");

    // FIXME

    // FIXME - actual I/O (WRITE [0x2a/0x8a])

    return nt_status::NOT_IMPLEMENTED;
}

static void init_lun(usb_device& d, xhci_queue& in_queue,
                     xhci_queue& out_queue, uint8_t lun) {
    auto pv = new usbms_volume(d, in_queue, out_queue, lun);

    if (!pv) {
        print("init_lun: new failed\n");
        return;
    }

    object_ref obj_ref{*pv};

    auto& v = *pv;

    if (!v.initialized) {
        print("init_lun: usbms_volume::usbms_volume failed\n");
        return;
    }

    // FIXME - give name
    // FIXME - print found message with name(?), block size, and size

    v.dev_num = num_usbms_devices.fetch_add(1);

    {
        char s[50];
        auto sp = span(s, sizeof(s));

        sp = format(sp, "ums{}", v.dev_num);

        auto sv = string_view(s, (size_t)(sp.data() - s));

        v.volume_name_len = sv.size();
        v.volume_name = (char*)malloc(sv.size());
        if (!v.volume_name) {
            print("malloc failed\n");
            return;
        }

        memcpy(v.volume_name, sv.data(), sv.size());

        auto status = name_object(v, sv);
        if (!success(status)) {
            print("name_object returned {}\n", status);
            return;
        }
    }

    print("found /dev/{}: {}, block size {}, capacity {}\n",
          string_view(v.volume_name, v.volume_name_len), v.model_number,
          byte_size(v.sector_size), byte_size(v.size));

    v.probe();

    // FIXME - handle removals
}

static bool found_usb_mass_storage(usb_device& d,
                                   const usb_interface& interface) {
    usb_endpoint* in_ep = nullptr;
    usb_endpoint* out_ep = nullptr;

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
