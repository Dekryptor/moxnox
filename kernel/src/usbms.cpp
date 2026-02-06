module;

#include <stdint.h>
#include <span>
#include <bit>
#include <string_view>
#include <atomic>
#include <mutex>

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
import rwsem;
import spinlock;
import vfs;

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
    WRITE10 = 0x2a,
    SYNCHRONIZE_CACHE = 0x35,
    READ16 = 0x88,
    WRITE16 = 0x8a,
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

struct scsi_read16 {
    enum scsi_command cmd;
    uint8_t dld2 : 1;
    uint8_t : 1;
    uint8_t rarc : 1;
    uint8_t fua : 1;
    uint8_t dpo : 1;
    uint8_t rdprotect : 3;
    uint64_t lba;
    uint32_t transfer_length;
    uint8_t group_number : 6;
    uint8_t dld0 : 1;
    uint8_t dld1 : 1;
    uint8_t control;
} __attribute((packed));

static_assert(sizeof(scsi_read16) == 16);

struct scsi_write10 {
    enum scsi_command cmd;
    uint8_t : 3;
    uint8_t fua : 1;
    uint8_t dpo : 1;
    uint8_t wrprotect : 3;
    uint32_t lba;
    uint8_t group_number : 5;
    uint8_t : 3;
    uint16_t transfer_length;
    uint8_t control;
} __attribute((packed));

static_assert(sizeof(scsi_write10) == 10);

struct scsi_write16 {
    enum scsi_command cmd;
    uint8_t dld2 : 1;
    uint8_t : 2;
    uint8_t fua : 1;
    uint8_t dpo : 1;
    uint8_t wrprotect : 3;
    uint64_t lba;
    uint32_t transfer_length;
    uint8_t group_number : 6;
    uint8_t dld0 : 1;
    uint8_t dld1 : 1;
    uint8_t control;
} __attribute((packed));

static_assert(sizeof(scsi_write16) == 16);

struct scsi_synchronize_cache {
    enum scsi_command cmd;
    uint8_t : 1;
    uint8_t immed : 1;
    uint8_t : 6;
    uint32_t lba;
    uint8_t group_number : 5;
    uint8_t : 3;
    uint16_t number_of_blocks;
    uint8_t control;
} __attribute((packed));

static_assert(sizeof(scsi_synchronize_cache) == 10);

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

class usbms_device {
public:
    usbms_device(usb_device& d, xhci_queue& in_queue,
                 xhci_queue& out_queue) : d(d), in_queue(in_queue), out_queue(out_queue) {
        d.acquire();

        d.removal_callback_ctx = this;
        d.removal_callback = [](void* ctx) {
            ((usbms_device*)ctx)->removal();
        };
    }

    ~usbms_device() {
        d.release();
    }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    void removal();
    bool send_scsi_msg_no_data(uint8_t lun, span<uint8_t> msg);
    size_t send_scsi_msg_in(uint8_t lun, span<uint8_t> msg, span<uint8_t> buf);
    size_t send_scsi_msg_in_dio(uint8_t lun, span<uint8_t> msg,
                                pfn_iterator& it, uint64_t num_pages);
    size_t send_scsi_msg_out_dio(uint8_t lun, span<uint8_t> msg,
                                 pfn_iterator& it, uint64_t num_pages);

    atomic<unsigned int> refcount = 1;
    usb_device& d;
    xhci_queue& in_queue;
    xhci_queue& out_queue;
    rwsem lock;
    uint32_t next_tag = 0;
    spinlock child_lock;
    list_entry children = { &children, &children };
};

static spinlock usbms_volumes_lock;
static list_entry usbms_volumes = { &usbms_volumes, &usbms_volumes };

class usbms_volume : public volume {
public:
    usbms_volume(usbms_device& dev, uint8_t lun);

    virtual ~usbms_volume() override {
        if (in_parent_list) {
            lock_guard lg(dev.child_lock);
            remove_entry_list(le);
        }

        {
            lock_guard lg(usbms_volumes_lock);

            if (in_global_list)
                remove_entry_list(global_le);
        }

        dev.release();
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
    void removal();

    bool initialized = false;
    unsigned int dev_num;
    char model_number_buf[16];
    string_view model_number;
    bool in_parent_list = false;
    list_entry le;
    bool in_global_list = false;
    list_entry global_le;
    bool dead = false;

private:
    bool test_unit_ready();
    bool inquiry();
    bool read_capacity10();
    bool read_capacity16();

    usbms_device& dev;
    uint8_t lun;
    bool big = false;
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

static bool usbms_get_max_lun(usb_device& d, uint8_t& max_lun,
                              uint16_t interface) {
    return d.ep0_queue.control_transfer(span<uint8_t>(&max_lun, 1),
                                        USB_REQUEST_TYPE_DEVICE_TO_HOST | USB_REQUEST_TYPE_CLASS | USB_REQUEST_TYPE_RECIPIENT_INTERFACE,
                                        (enum usb_request)usbms_request::GET_MAX_LUN,
                                        0, interface);
}

bool usbms_device::send_scsi_msg_no_data(uint8_t lun, span<uint8_t> msg) {
    usbms_cbw cbw;
    usbms_csw csw;

    lock_guard lg(lock);

    // FIXME - translate into nt_status?
    // FIXME - handle stalls

    // prepare command

    memset(&cbw, 0, sizeof(cbw));

    cbw.dCBWSignature = USBMS_CBW_SIG;
    cbw.dCBWTag = next_tag;
    cbw.dCBWDataTransferLength = 0;
    cbw.bmCBWFlags = 0;
    cbw.bCBWLUN = lun;
    cbw.bCBWCBLength = sizeof(cbw.msg);

    next_tag++;

    memcpy(&cbw.msg, msg.data(), msg.size());

    // send command

    auto sent_bytes = out_queue.normal_transfer(span((uint8_t*)&cbw, sizeof(cbw)));

    if (sent_bytes == 0) {
        print("usbms_device::send_scsi_msg_no_data: xhci_queue::normal_transfer failed for OUT endpoint\n");
        return false;
    } else if (sent_bytes != sizeof(cbw)) {
        print("usbms_device::send_scsi_msg_no_data: xhci_queue::normal_transfer sent {} bytes, expected {}\n",
              sent_bytes, sizeof(cbw));
        return false;
    }

    // wait for reply

    auto recv_bytes = in_queue.normal_transfer(span((uint8_t*)&csw, sizeof(csw)));

    if (recv_bytes == 0) {
        print("usbms_device::send_scsi_msg_no_data: xhci_queue::normal_transfer failed for IN endpoint\n");
        return false;
    } else if (recv_bytes != sizeof(csw)) {
        print("usbms_device::send_scsi_msg_no_data: xhci_queue::normal_transfer received {} bytes, expected {}\n",
              recv_bytes, sizeof(csw));
        return false;
    }

    if (csw.dCSWSignature != USBMS_CSW_SIG) {
        print("send_scsi_msg_no_data: response had invalid signature ({:08x}, expected {:08x})\n",
              csw.dCSWSignature, USBMS_CSW_SIG);
        return false;
    }

    if (csw.dCSWTag != cbw.dCBWTag) {
        print("usbms_device::send_scsi_msg_no_data: response had invalid tag ({:02x}, expected {:02x})\n",
              csw.dCSWTag, cbw.dCBWTag);
        return false;
    }

    switch (csw.bCSWStatus) {
        case csw_status::success:
            return true;

        case csw_status::failed:
            print("usbms_device::send_scsi_msg_no_data: command failed\n");
            return false;

        case csw_status::phase_error:
            print("usbms_device::send_scsi_msg_no_data: phase error\n");
            return false;

        default:
            print("usbms_device::send_scsi_msg_no_data: unrecognized error status {:02x}\n",
                  (uint8_t)csw.bCSWStatus);
            return false;
    }
}

size_t usbms_device::send_scsi_msg_in(uint8_t lun, span<uint8_t> msg,
                                      span<uint8_t> buf) {
    usbms_cbw cbw;
    usbms_csw csw;

    lock_guard lg(lock);

    // FIXME - translate into nt_status?
    // FIXME - handle stalls

    // prepare command

    memset(&cbw, 0, sizeof(cbw));

    cbw.dCBWSignature = USBMS_CBW_SIG;
    cbw.dCBWTag = next_tag;
    cbw.dCBWDataTransferLength = (uint32_t)buf.size();
    cbw.bmCBWFlags = 0x80; // in
    cbw.bCBWLUN = lun;
    cbw.bCBWCBLength = sizeof(cbw.msg);

    next_tag++;

    memcpy(&cbw.msg, msg.data(), msg.size());

    // send command

    auto sent_bytes = out_queue.normal_transfer(span((uint8_t*)&cbw, sizeof(cbw)));

    if (sent_bytes == 0) {
        print("usbms_device::send_scsi_msg_in: xhci_queue::normal_transfer failed for OUT endpoint\n");
        return 0;
    } else if (sent_bytes != sizeof(cbw)) {
        print("usbms_device::send_scsi_msg_in: xhci_queue::normal_transfer sent {} bytes, expected {}\n",
              sent_bytes, sizeof(cbw));
        return 0;
    }

    // wait for data

    auto recv_bytes = in_queue.normal_transfer(span(buf.data(), buf.size()));

    if (recv_bytes == 0) {
        print("usbms_device::send_scsi_msg_in: xhci_queue::normal_transfer failed for IN endpoint\n");
        return 0;
    } else if (recv_bytes != buf.size()) {
        print("usbms_device::send_scsi_msg_in: xhci_queue::normal_transfer received {} bytes, expected {}\n",
              recv_bytes, buf.size());
        return 0;
    }

    recv_bytes = in_queue.normal_transfer(span((uint8_t*)&csw, sizeof(csw)));

    if (recv_bytes == 0) {
        print("usbms_device::send_scsi_msg_in: xhci_queue::normal_transfer failed for IN endpoint\n");
        return 0;
    } else if (recv_bytes != sizeof(csw)) {
        print("usbms_device::send_scsi_msg_in: xhci_queue::normal_transfer received {} bytes, expected {}\n",
              recv_bytes, sizeof(csw));
        return 0;
    }

    // wait for status

    if (csw.dCSWSignature != USBMS_CSW_SIG) {
        print("usbms_device::send_scsi_msg_in: response had invalid signature ({:08x}, expected {:08x})\n",
              csw.dCSWSignature, USBMS_CSW_SIG);
        return 0;
    }

    if (csw.dCSWTag != cbw.dCBWTag) {
        print("usbms_device::send_scsi_msg_in: response had invalid tag ({:02x}, expected {:02x})\n",
              csw.dCSWTag, cbw.dCBWTag);
        return 0;
    }

    switch (csw.bCSWStatus) {
        case csw_status::success:
            return buf.size() - csw.dCSWDataResidue;

        case csw_status::failed:
            print("usbms_device::send_scsi_msg_in: command failed\n");
            return 0;

        case csw_status::phase_error:
            print("usbms_device::send_scsi_msg_in: phase error\n");
            return 0;

        default:
            print("usbms_device::send_scsi_msg_in: unrecognized error status {:02x}\n",
                  (uint8_t)csw.bCSWStatus);
            return 0;
    }
}

size_t usbms_device::send_scsi_msg_in_dio(uint8_t lun, span<uint8_t> msg,
                                          pfn_iterator& it, uint64_t num_pages) {
    usbms_cbw cbw;
    usbms_csw csw;

    lock_guard lg(lock);

    // FIXME - translate into nt_status?
    // FIXME - handle stalls

    // prepare command

    memset(&cbw, 0, sizeof(cbw));

    cbw.dCBWSignature = USBMS_CBW_SIG;
    cbw.dCBWTag = next_tag;
    cbw.dCBWDataTransferLength = (uint32_t)(num_pages << PAGE_SHIFT);
    cbw.bmCBWFlags = 0x80; // in
    cbw.bCBWLUN = lun;
    cbw.bCBWCBLength = sizeof(cbw.msg);

    next_tag++;

    memcpy(&cbw.msg, msg.data(), msg.size());

    // send command

    auto sent_bytes = out_queue.normal_transfer(span((uint8_t*)&cbw, sizeof(cbw)));

    if (sent_bytes == 0) {
        print("usbms_device::send_scsi_msg_in_dio: xhci_queue::normal_transfer failed for OUT endpoint\n");
        return 0;
    } else if (sent_bytes != sizeof(cbw)) {
        print("usbms_device::send_scsi_msg_in_dio: xhci_queue::normal_transfer sent {} bytes, expected {}\n",
              sent_bytes, sizeof(cbw));
        return 0;
    }

    // wait for data

    auto recv_bytes = in_queue.normal_transfer_dio(it, num_pages);
    if (recv_bytes == 0) {
        print("usbms_device::send_scsi_msg_in_dio: xhci_queue::normal_transfer failed for IN endpoint\n");
        return 0;
    } else if (recv_bytes != num_pages << PAGE_SHIFT) {
        print("usbms_device::send_scsi_msg_in_dio: xhci_queue::normal_transfer received {} bytes, expected {}\n",
                recv_bytes, num_pages << PAGE_SHIFT);
        return 0;
    }

    recv_bytes = in_queue.normal_transfer(span((uint8_t*)&csw, sizeof(csw)));

    if (recv_bytes == 0) {
        print("usbms_device::send_scsi_msg_in_dio: xhci_queue::normal_transfer failed for IN endpoint\n");
        return 0;
    } else if (recv_bytes != sizeof(csw)) {
        print("usbms_device::send_scsi_msg_in_dio: xhci_queue::normal_transfer received {} bytes, expected {}\n",
              recv_bytes, sizeof(csw));
        return 0;
    }

    // wait for status

    if (csw.dCSWSignature != USBMS_CSW_SIG) {
        print("usbms_device::send_scsi_msg_in_dio: response had invalid signature ({:08x}, expected {:08x})\n",
              csw.dCSWSignature, USBMS_CSW_SIG);
        return 0;
    }

    if (csw.dCSWTag != cbw.dCBWTag) {
        print("usbms_device::send_scsi_msg_in_dio: response had invalid tag ({:02x}, expected {:02x})\n",
              csw.dCSWTag, cbw.dCBWTag);
        return 0;
    }

    switch (csw.bCSWStatus) {
        case csw_status::success:
            return (num_pages << PAGE_SHIFT) - csw.dCSWDataResidue;

        case csw_status::failed:
            print("usbms_device::send_scsi_msg_in_dio: command failed\n");
            return 0;

        case csw_status::phase_error:
            print("usbms_device::send_scsi_msg_in_dio: phase error\n");
            return 0;

        default:
            print("usbms_device::send_scsi_msg_in_dio: unrecognized error status {:02x}\n",
                  (uint8_t)csw.bCSWStatus);
            return 0;
    }
}

size_t usbms_device::send_scsi_msg_out_dio(uint8_t lun, span<uint8_t> msg,
                                           pfn_iterator& it, uint64_t num_pages) {
    usbms_cbw cbw;
    usbms_csw csw;

    lock_guard lg(lock);

    // FIXME - translate into nt_status?
    // FIXME - handle stalls

    // prepare command

    memset(&cbw, 0, sizeof(cbw));

    cbw.dCBWSignature = USBMS_CBW_SIG;
    cbw.dCBWTag = next_tag;
    cbw.dCBWDataTransferLength = (uint32_t)(num_pages << PAGE_SHIFT);
    cbw.bmCBWFlags = 0; // out
    cbw.bCBWLUN = lun;
    cbw.bCBWCBLength = sizeof(cbw.msg);

    next_tag++;

    memcpy(&cbw.msg, msg.data(), msg.size());

    // send command

    auto sent_bytes = out_queue.normal_transfer(span((uint8_t*)&cbw, sizeof(cbw)));

    if (sent_bytes == 0) {
        print("usbms_device::send_scsi_msg_out_dio: xhci_queue::normal_transfer failed for OUT endpoint\n");
        return 0;
    } else if (sent_bytes != sizeof(cbw)) {
        print("usbms_device::send_scsi_msg_out_dio: xhci_queue::normal_transfer sent {} bytes, expected {}\n",
              sent_bytes, sizeof(cbw));
        return 0;
    }

    // send data

    sent_bytes = out_queue.normal_transfer_dio(it, num_pages);
    if (sent_bytes == 0) {
        print("usbms_device::send_scsi_msg_out_dio: xhci_queue::normal_transfer failed for OUT endpoint\n");
        return 0;
    } else if (sent_bytes != num_pages << PAGE_SHIFT) {
        print("usbms_device::send_scsi_msg_out_dio: xhci_queue::normal_transfer sent {} bytes, expected {}\n",
              sent_bytes, num_pages << PAGE_SHIFT);
        return 0;
    }

    // receive status

    auto recv_bytes = in_queue.normal_transfer(span((uint8_t*)&csw, sizeof(csw)));

    if (recv_bytes == 0) {
        print("usbms_device::send_scsi_msg_out_dio: xhci_queue::normal_transfer failed for IN endpoint\n");
        return 0;
    } else if (recv_bytes != sizeof(csw)) {
        print("usbms_device::send_scsi_msg_out_dio: xhci_queue::normal_transfer received {} bytes, expected {}\n",
              recv_bytes, sizeof(csw));
        return 0;
    }

    if (csw.dCSWSignature != USBMS_CSW_SIG) {
        print("usbms_device::send_scsi_msg_out_dio: response had invalid signature ({:08x}, expected {:08x})\n",
              csw.dCSWSignature, USBMS_CSW_SIG);
        return 0;
    }

    if (csw.dCSWTag != cbw.dCBWTag) {
        print("usbms_device::send_scsi_msg_out_dio: response had invalid tag ({:02x}, expected {:02x})\n",
              csw.dCSWTag, cbw.dCBWTag);
        return 0;
    }

    switch (csw.bCSWStatus) {
        case csw_status::success:
            return (num_pages << PAGE_SHIFT) - csw.dCSWDataResidue;

        case csw_status::failed:
            print("usbms_device::send_scsi_msg_out_dio: command failed\n");
            return 0;

        case csw_status::phase_error:
            print("usbms_device::send_scsi_msg_out_dio: phase error\n");
            return 0;

        default:
            print("usbms_device::send_scsi_msg_out_dio: unrecognized error status {:02x}\n",
                  (uint8_t)csw.bCSWStatus);
            return 0;
    }
}

void usbms_volume::removal() {
    dead = true;

    surprise_removal(*this);

    remove_object_name(*this);
    remove_partitions(*this);
}

void usbms_device::removal() {
    while (true) {
        usbms_volume* v;

        {
            lock_guard lg(child_lock);

            if (is_list_empty(children))
                break;

            v = &containing_record<usbms_volume>(children.next);

            remove_entry_list(v->le);
            v->in_parent_list = false;
        }

        {
            lock_guard lg(usbms_volumes_lock);

            if (v->in_global_list)
                remove_entry_list(v->global_le);
        }

        v->acquire();
        v->removal();
        v->release();
    }
}

bool usbms_volume::test_unit_ready() {
    scsi_test_unit_ready msg;

    memset(&msg, 0, sizeof(msg));
    msg.cmd = scsi_command::TEST_UNIT_READY;

    return dev.send_scsi_msg_no_data(lun, span((uint8_t*)&msg, sizeof(msg)));
}

bool usbms_volume::inquiry() {
    scsi_inquiry_cmd msg;
    scsi_inquiry buf;

    static const size_t MIN_INQUIRY_SIZE = 36;

    memset(&msg, 0, sizeof(msg));
    msg.cmd = scsi_command::INQUIRY;
    msg.allocation_length = byteswap((uint16_t)sizeof(buf));

    auto recv_bytes = dev.send_scsi_msg_in(lun,
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

    auto recv_bytes = dev.send_scsi_msg_in(lun,
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

    auto recv_bytes = dev.send_scsi_msg_in(lun,
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

usbms_volume::usbms_volume(usbms_device& dev, uint8_t lun) : dev(dev), lun(lun) {
    dev.acquire();

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

    {
        lock_guard lg(dev.child_lock);
        insert_tail_list(dev.children, le);
    }

    in_parent_list = true;

    initialized = true;
}

nt_status usbms_volume::flush() {
    if (dead)
        return nt_status::DEVICE_REMOVED;

    scsi_synchronize_cache msg;

    memset(&msg, 0, sizeof(msg));
    msg.cmd = scsi_command::SYNCHRONIZE_CACHE;

    if (!dev.send_scsi_msg_no_data(lun, span((uint8_t*)&msg, sizeof(msg))))
        return nt_status::IO_DEVICE_ERROR;

    return nt_status::SUCCESS;
}

nt_status usbms_volume::dio_read(uint64_t page_num, uint64_t num_pages,
                                 pfn_iterator& it, uint64_t& read_pages) {
    if (dead)
        return nt_status::DEVICE_REMOVED;

    if (num_pages == 0) {
        read_pages = 0;
        return nt_status::SUCCESS;
    }

    num_pages = min((uint64_t)dev.in_queue.size, num_pages);

    size_t recv_bytes;

    if (big) {
        scsi_read16 msg;

        // FIXME - test this

        memset(&msg, 0, sizeof(msg));
        msg.cmd = scsi_command::READ16;
        msg.lba = byteswap((uint64_t)((page_num << PAGE_SHIFT) / sector_size));
        msg.transfer_length = byteswap((uint32_t)(num_pages * (PAGE_SIZE / sector_size)));

        recv_bytes = dev.send_scsi_msg_in_dio(lun,
                                              span((uint8_t*)&msg, sizeof(msg)),
                                              it, num_pages);
    } else {
        scsi_read10 msg;

        memset(&msg, 0, sizeof(msg));
        msg.cmd = scsi_command::READ10;
        msg.lba = byteswap((uint32_t)((page_num << PAGE_SHIFT) / sector_size));
        msg.transfer_length = byteswap((uint16_t)(num_pages * (PAGE_SIZE / sector_size)));

        recv_bytes = dev.send_scsi_msg_in_dio(lun,
                                              span((uint8_t*)&msg, sizeof(msg)),
                                              it, num_pages);
    }

    if (recv_bytes != num_pages << PAGE_SHIFT) {
        print("usbms_volume::dio_read: received {} bytes, expected {}\n",
              recv_bytes, num_pages << PAGE_SHIFT);
        return nt_status::IO_DEVICE_ERROR;
    }

    read_pages = num_pages;

    return nt_status::SUCCESS;
}

nt_status usbms_volume::dio_write(uint64_t page_num, uint64_t num_pages,
                                  pfn_iterator& it, uint64_t& written_pages,
                                  bool fua) {
    if (dead)
        return nt_status::DEVICE_REMOVED;

    if (num_pages == 0) {
        written_pages = 0;
        return nt_status::SUCCESS;
    }

    num_pages = min((uint64_t)dev.out_queue.size, num_pages);

    if (big) {
        scsi_write16 msg;

        // FIXME - test this

        memset(&msg, 0, sizeof(msg));
        msg.cmd = scsi_command::WRITE16;
        msg.lba = byteswap((uint32_t)((page_num << PAGE_SHIFT) / sector_size));
        msg.transfer_length = byteswap((uint16_t)(num_pages * (PAGE_SIZE / sector_size)));
        msg.fua = fua ? 1 : 0;

        if (!dev.send_scsi_msg_out_dio(lun, span((uint8_t*)&msg, sizeof(msg)),
                                       it, num_pages)) {
            print("usbms_volume::dio_write: send_scsi_msg_out_dio failed\n");
            return nt_status::IO_DEVICE_ERROR;
        }
    } else {
        scsi_write10 msg;

        memset(&msg, 0, sizeof(msg));
        msg.cmd = scsi_command::WRITE10;
        msg.lba = byteswap((uint32_t)((page_num << PAGE_SHIFT) / sector_size));
        msg.transfer_length = byteswap((uint16_t)(num_pages * (PAGE_SIZE / sector_size)));
        msg.fua = fua ? 1 : 0;

        if (!dev.send_scsi_msg_out_dio(lun, span((uint8_t*)&msg, sizeof(msg)),
                                       it, num_pages)) {
            print("usbms_volume::dio_write: send_scsi_msg_out_dio failed\n");
            return nt_status::IO_DEVICE_ERROR;
        }
    }

    written_pages = num_pages;

    return nt_status::SUCCESS;
}

static unsigned int find_free_dev_num() {
    unsigned int num = 0;

    // FIXME - surely there's a better way to do this

    while (true) {
        bool collision = false;

        for (auto le = usbms_volumes.next; le != &usbms_volumes; le = le->next) {
            auto& v = *(usbms_volume*)((uint8_t*)le - offsetof(usbms_volume, global_le));

            if (v.dev_num == num) {
                collision = true;
                break;
            }
        }

        if (!collision)
            return num;

        num++;
    }
}

static void init_lun(usbms_device& dev, uint8_t lun) {
    auto pv = new usbms_volume(dev, lun);

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

    {
        lock_guard lock(usbms_volumes_lock);

        v.dev_num = find_free_dev_num();

        insert_tail_list(usbms_volumes, v.global_le);
        v.in_global_list = true;
    }

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

    auto dev = new usbms_device(d, *in_queue, *out_queue);
    if (!dev) {
        print("new failed\n");
        return false;
    }

    for (uint8_t i = 0; i <= max_lun; i++) {
        init_lun(*dev, i);
    }

    dev->release();

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
