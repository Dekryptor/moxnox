#pragma once

struct xhci_cap_regs {
    uint8_t CAPLENGTH;
    uint8_t reserved;
    uint16_t HCIVERSION;
    uint32_t HCSPARAMS1;
    uint32_t HCSPARAMS2;
    uint32_t HCSPARAMS3;
    uint32_t HCCPARAMS1;
    uint32_t DBOFF;
    uint32_t RTSOFF;
    uint32_t HCCPARAMS2;
} __attribute((packed));

static_assert(sizeof(xhci_cap_regs) == 0x20);

struct xhci_port_regs {
    uint32_t PORTSC;
    uint32_t PORTPMSC;
    uint32_t PORTLI;
    uint32_t PORTHLPMC;
};

static_assert(sizeof(xhci_port_regs) == 0x10);

static const uint32_t XHCI_PORTSC_CCS = 0x00000001;
static const uint32_t XHCI_PORTSC_PED = 0x00000002;
static const uint32_t XHCI_PORTSC_PR = 0x00000010;
static const uint32_t XHCI_PORTSC_PP = 0x00000200;
static const uint32_t XHCI_PORTSC_PIC = 0x0000c000;

struct xhci_op_regs {
    uint32_t USBCMD;
    uint32_t USBSTS;
    uint32_t PAGESIZE;
    uint32_t reserved1[2];
    uint32_t DNCTRL;
    uint64_t CRCR;
    uint32_t reserved2[4];
    uint64_t DCBAAP;
    uint32_t CONFIG;
    uint8_t reserved3[964];
    xhci_port_regs ports[256];
} __attribute((packed));

static_assert(sizeof(xhci_op_regs) == 0x1400);

static const uint32_t XHCI_USBCMD_RS = 0x00000001;
static const uint32_t XHCI_USBCMD_HCRST = 0x00000002;
static const uint32_t XHCI_USBCMD_INTE = 0x00000004;

struct xhci_int_regs {
    uint32_t IMAN;
    uint32_t IMOD;
    uint32_t ERSTSZ;
    uint32_t reserved;
    uint64_t ERSTBA;
    uint64_t ERDP;
} __attribute((packed));

static_assert(sizeof(xhci_int_regs) == 0x20);

static const uint32_t XHCI_IMAN_IE = 0x00000002;

static const uint32_t XHCI_ERDP_EHB = 0x00000008;

static const size_t MAX_INT_REGS = 1024;

struct xhci_run_regs {
    uint32_t MFINDEX;
    uint8_t reserved[28];
    xhci_int_regs int_regs[MAX_INT_REGS];
} __attribute((packed));

static_assert(sizeof(xhci_run_regs) == 0x8020);

struct xhci_event_ring_segment {
    uint64_t base_address;
    uint16_t size;
    uint16_t reserved1;
    uint32_t reserved2;
} __attribute((packed));

static_assert(sizeof(xhci_event_ring_segment) == 0x10);

enum class xhci_trb_type {
    normal = 1,
    setup_stage = 2,
    data_stage = 3,
    status_stage = 4,
    isoch = 5,
    link = 6,
    event_data = 7,
    nop_transfer = 8,
    enable_slot_command = 9,
    disable_slot_command = 10,
    address_device_command =11 ,
    configure_endpoint_command = 12,
    evaluate_context_command = 13,
    reset_endpoint_command = 14,
    stop_endpoint_command = 15,
    set_tr_dequeue_pointer_command = 16,
    reset_device_command = 17,
    force_event_command = 18,
    negotiate_bandwidth_command = 19,
    set_latency_tolerance_value_command = 20,
    get_port_bandwidth_command = 21,
    force_header_command = 22,
    nop_command = 23,
    get_extended_property_command = 24,
    set_extended_property_command = 25,
    transfer_event = 32,
    command_completion_event = 33,
    port_status_change_event = 34,
    bandwidth_request_event = 35,
    doorbell_event = 36,
    host_controller_event = 37,
    device_notification_event = 38,
    mfindex_wrap_event = 39
};

struct xhci_trb_common {
    uint32_t reserved1[3];
    uint32_t cycle : 1;
    uint32_t reserved2 : 9;
    uint32_t trb_type : 6;
    uint32_t slot_type : 5;
    uint32_t reserved3 : 11;
} __attribute((packed));

static_assert(sizeof(xhci_trb_common) == 0x10);

struct xhci_trb_enable_slot {
    uint32_t reserved1[3];
    uint32_t cycle : 1;
    uint32_t reserved2 : 9;
    uint32_t trb_type : 6;
    uint32_t slot_type : 5;
    uint32_t reserved3 : 11;
} __attribute((packed));

static_assert(sizeof(xhci_trb_enable_slot) == sizeof(xhci_trb_common));

struct xhci_trb_command_completion {
    uint64_t command_trb_pointer;
    uint32_t command_completion_parameter : 24;
    uint32_t completion_code : 8;
    uint16_t cycle : 1;
    uint16_t reserved : 9;
    uint16_t trb_type : 6;
    uint8_t vfid;
    uint8_t slot_id;
} __attribute((packed));

static_assert(sizeof(xhci_trb_command_completion) == sizeof(xhci_trb_common));
