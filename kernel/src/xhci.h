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

static const uint32_t XHCI_HCCPARAMS1_XECP = 0xffff0000;
static const size_t XHCI_HCCPARAMS1_XECP_SHIFT = 16;
static const uint32_t XHCI_HCCPARAMS1_CSZ = 0x00000004;

struct xhci_port_regs {
    uint32_t PORTSC;
    uint32_t PORTPMSC;
    uint32_t PORTLI;
    uint32_t PORTHLPMC;
};

static_assert(sizeof(xhci_port_regs) == 0x10);

static const uint32_t XHCI_PORTSC_PP = 0x00000200;
static const uint32_t XHCI_PORTSC_CSC = 0x00020000;
static const uint32_t XHCI_PORTSC_PRC = 0x00200000;

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

static const uint32_t XHCI_IMAN_IP = 0x00000001;
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
    address_device_command = 11,
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

struct xhci_trb_disable_slot {
    uint32_t reserved1[3];
    uint16_t cycle : 1;
    uint16_t reserved2 : 9;
    uint16_t trb_type : 6;
    uint8_t reserved3;
    uint8_t slot_id;
} __attribute((packed));

static_assert(sizeof(xhci_trb_disable_slot) == sizeof(xhci_trb_common));

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

enum class TRB_CODE {
    INVALID = 0,
    SUCCESS = 1,
    DATA_BUFFER_ERROR = 2,
    BABBLE_DETECTED = 3,
    USB_TRANSACTION_ERROR = 4,
    TRB_ERROR = 5,
    STALL_ERROR = 6,
    RESOURCE_ERROR = 7,
    BANDWIDTH_ERROR = 8,
    NO_SLOTS_AVAILABLE_ERROR = 9,
    INVALID_STREAM_TYPE = 10,
    SLOT_NOT_ENABLED = 11,
    ENDPOINT_NOT_ENABLED = 12,
    SHORT_PACKET = 13,
    RING_UNDERRUN = 14,
    RING_OVERRUN = 15,
    VF_EVENT_RING_FULL = 16,
    PARAMETER_ERROR = 17,
    BANDWIDTH_OVERRUN = 18,
    CONTEXT_STATE_ERROR = 19,
    NO_PINT_RESPONSE = 20,
    EVENT_RING_FULL = 21,
    INCOMPATIBLE_DEVICE = 22,
    MISSED_SERVICE = 23,
    COMMAND_RING_STOPPED = 24,
    COMMAND_ABORTED = 25,
    STOPPED = 26,
    STOPPED_LENGTH_INVALID = 27,
    STOPPED_SHORT_PACKET = 28,
    MAX_EXIT_LATENCY_TOO_LARGE = 29,
    ISOCH_BUFFER_OVERRUN = 31,
    EVENT_LOST = 32,
    UNDEFINED_ERROR = 33,
    INVALID_STREAM_ID = 34,
    SECONDARY_BANDWIDTH_ERROR = 35,
    SPLIT_TRANSACTION_ERROR = 36,
};

struct xhci_trb_link {
    uint64_t ring_segment_pointer;
    uint32_t reserved1 : 22;
    uint32_t interrupter_target : 10;
    uint16_t cycle : 1;
    uint16_t toggle_cycle : 1;
    uint16_t reserved2 : 2;
    uint16_t chain : 1;
    uint16_t interrupt_on_completion : 1;
    uint16_t reserved3 : 4;
    uint16_t trb_type : 6;
    uint16_t reserved4;
} __attribute((packed));

static_assert(sizeof(xhci_trb_link) == sizeof(xhci_trb_common));

struct xhci_trb_port_status_change {
    uint8_t reserved1[3];
    uint8_t port_id;
    uint32_t reserved2;
    uint8_t reserved3[3];
    uint8_t completion_code;
    uint16_t cycle : 1;
    uint16_t reserved4 : 9;
    uint16_t trb_type : 6;
    uint16_t reserved5;
} __attribute((packed));

static_assert(sizeof(xhci_trb_port_status_change) == sizeof(xhci_trb_common));

struct xhci_trb_address_device {
    uint64_t input_context_pointer;
    uint32_t reserved1;
    uint16_t cycle : 1;
    uint16_t reserved2 : 8;
    uint16_t bsr : 1;
    uint16_t trb_type : 6;
    uint8_t reserved3;
    uint8_t slot_id;
} __attribute((packed));

static_assert(sizeof(xhci_trb_address_device) == sizeof(xhci_trb_common));

struct xhci_trb_setup_stage {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
    uint32_t trb_transfer_length : 17;
    uint32_t reserved1 : 5;
    uint32_t interrupter_target : 10;
    uint32_t cycle : 1;
    uint32_t reserved2 : 4;
    uint32_t ioc : 1;
    uint32_t idt : 1;
    uint32_t reserved3 : 3;
    uint32_t trb_type : 6;
    uint32_t transfer_type : 2;
    uint32_t reserved4 : 14;
} __attribute((packed));

static_assert(sizeof(xhci_trb_setup_stage) == sizeof(xhci_trb_common));

struct xhci_trb_data_stage {
    uint64_t data_buffer;
    uint32_t trb_transfer_length : 17;
    uint32_t td_size : 5;
    uint32_t interrupt_target : 10;
    uint16_t cycle : 1;
    uint16_t ent : 1;
    uint16_t isp : 1;
    uint16_t ns : 1;
    uint16_t ch : 1;
    uint16_t ioc : 1;
    uint16_t idt : 1;
    uint16_t reserved1 : 3;
    uint16_t trb_type : 6;
    uint16_t dir : 1;
    uint16_t reserved2 : 15;
} __attribute((packed));

static_assert(sizeof(xhci_trb_data_stage) == sizeof(xhci_trb_common));

struct xhci_trb_status_stage {
    uint32_t reserved1[2];
    uint32_t reserved2 : 22;
    uint32_t interrupter_target : 10;
    uint16_t cycle : 1;
    uint16_t ent : 1;
    uint16_t reserved3 : 2;
    uint16_t ch : 1;
    uint16_t ioc : 1;
    uint16_t reserved4 : 4;
    uint16_t trb_type : 6;
    uint16_t dir : 1;
    uint16_t reserved5 : 15;
} __attribute((packed));

static_assert(sizeof(xhci_trb_status_stage) == sizeof(xhci_trb_common));

struct xhci_trb_transfer_event {
    uint64_t trb_pointer;
    uint32_t trb_transfer_length : 24;
    uint32_t completion_code : 8;
    uint16_t cycle : 1;
    uint16_t reserved1 : 1;
    uint16_t event_data : 1;
    uint16_t reserved2 : 7;
    uint16_t trb_type : 6;
    uint8_t endpoint_id : 5;
    uint8_t reserved3 : 3;
    uint8_t slot_id;
} __attribute((packed));

static_assert(sizeof(xhci_trb_transfer_event) == sizeof(xhci_trb_common));

struct xhci_trb_configure_endpoint {
    uint64_t input_context_pointer;
    uint32_t reserved1;
    uint16_t cycle : 1;
    uint16_t reserved2 : 8;
    uint16_t deconfigure : 1;
    uint16_t trb_type : 6;
    uint8_t reserved3;
    uint8_t slot_id;
} __attribute((packed));

static_assert(sizeof(xhci_trb_configure_endpoint) == sizeof(xhci_trb_common));

struct xhci_trb_normal {
    uint64_t data_buffer;
    uint32_t trb_transfer_length : 17;
    uint32_t td_size : 5;
    uint32_t interrupter_target : 10;
    uint16_t cycle : 1;
    uint16_t ent : 1;
    uint16_t isp : 1;
    uint16_t ns : 1;
    uint16_t ch : 1;
    uint16_t ioc : 1;
    uint16_t idt : 1;
    uint16_t reserved1 : 2;
    uint16_t bei : 1;
    uint16_t trb_type : 6;
    uint16_t reserved2;
} __attribute((packed));

static_assert(sizeof(xhci_trb_normal) == sizeof(xhci_trb_common));

struct xhci_slot_context {
    uint32_t route_string : 20;
    uint32_t speed : 4;
    uint32_t reserved1 : 1;
    uint32_t multi_tt : 1;
    uint32_t hub : 1;
    uint32_t context_entries : 5;
    uint16_t max_exit_latency;
    uint8_t root_hub_port_number;
    uint8_t num_ports;
    uint8_t parent_hub_slot_id;
    uint8_t parent_port_number;
    uint16_t ttt : 2;
    uint16_t reserved2 : 4;
    uint16_t interrupter_target : 10;
    uint8_t usb_device_address;
    uint16_t reserved3;
    uint8_t reserved4 : 3;
    uint8_t slot_state : 5;
    uint32_t reserved5[4];
} __attribute((packed));

static_assert(sizeof(xhci_slot_context) == 0x20);

template<bool LARGE>
struct xhci_input_control_context {
    uint32_t drop_context;
    uint32_t add_context;
    uint32_t reserved1[5];
    uint8_t configuration_value;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t reserved2;
    uint8_t reserved3[LARGE ? 32 : 0];
} __attribute((packed));

static_assert(sizeof(xhci_input_control_context<false>) == 0x20);
static_assert(sizeof(xhci_input_control_context<true>) == 0x40);

template<bool LARGE>
struct xhci_endpoint_context {
    uint8_t ep_state : 3;
    uint8_t reserved1 : 5;
    uint8_t mult : 2;
    uint8_t maxpstreams : 5;
    uint8_t lsa : 1;
    uint8_t interval;
    uint8_t max_esit_payload_high;
    uint8_t reserved2 : 1;
    uint8_t cerr : 2;
    uint8_t ep_type : 3;
    uint8_t reserved3 : 1;
    uint8_t host_initiate_disable : 1;
    uint8_t max_burst_size;
    uint16_t max_packet_size;
    uint64_t tr_dequeue_pointer;
    uint16_t average_trb_length;
    uint16_t max_esit_payload_low;
    uint32_t reserved4[3];
    uint8_t reserved5[LARGE ? 32 : 0];
} __attribute((packed));

static_assert(sizeof(xhci_endpoint_context<false>) == 0x20);
static_assert(sizeof(xhci_endpoint_context<true>) == 0x40);

enum class XHCI_EP_TYPE {
    ISOCH_OUT = 1,
    BULK_OUT = 2,
    INTERRUPT_OUT = 3,
    CONTROL = 4,
    ISOCH_IN = 5,
    BULK_IN = 6,
    INTERRUPT_IN = 7,
};

static const uint32_t XHCI_EP_DCS = 0x00000001;

template<bool LARGE>
struct xhci_device_context {
    xhci_slot_context slot;
    uint8_t reserved[LARGE ? 32 : 0];
    xhci_endpoint_context<LARGE> ep[31];
} __attribute((packed));

static_assert(sizeof(xhci_device_context<false>) == 0x400);
static_assert(sizeof(xhci_device_context<true>) == 0x800);

template<bool LARGE>
struct xhci_input_context {
    xhci_input_control_context<LARGE> control;
    xhci_slot_context slot;
    uint8_t reserved1[LARGE ? 32 : 0];
    xhci_endpoint_context<LARGE> ep[31];
} __attribute((packed));

static_assert(sizeof(xhci_input_context<false>) == 0x420);
static_assert(sizeof(xhci_input_context<true>) == 0x840);

enum class xhci_capability_type : uint8_t {
    usb_legacy_support = 1,
    supported_protocol = 2,
    extended_power_management = 3,
    io_virtualization = 4,
    message_interrupt = 5,
    local_memory = 6,
    usb_debug_capability = 10,
    extended_message_interrupt = 17
};

struct xhci_extended_capability {
    enum xhci_capability_type cap_type;
    uint8_t next_cap;
    uint16_t cap_spec;
} __attribute((packed));

static_assert(sizeof(xhci_extended_capability) == 0x4);

struct xhci_supported_protocol {
    uint32_t cap_id : 8;
    uint32_t next_cap : 8;
    uint32_t revision_minor : 8;
    uint32_t revision_major : 8;
    uint32_t name_string;
    uint32_t port_offset : 8;
    uint32_t port_count : 8;
    uint32_t protocol_defined : 12;
    uint32_t psic : 4;
    uint32_t slot_type : 5;
    uint32_t reserved : 27;
} __attribute((packed));

static_assert(sizeof(xhci_supported_protocol) == 0x10);

struct xhci_hcsparams1 {
    uint32_t max_slots : 8;
    uint32_t max_interrupters : 11;
    uint32_t reserved : 5;
    uint32_t max_ports : 8;
} __attribute((packed));

static_assert(sizeof(xhci_hcsparams1) == 0x4);

struct xhci_hcsparams2 {
    uint32_t ist : 4;
    uint32_t erst_max : 4;
    uint32_t reserved : 13;
    uint32_t max_scratchpad_buffers_high : 5;
    uint32_t spr : 1;
    uint32_t max_scratchpad_buffers_low : 5;
} __attribute((packed));

static_assert(sizeof(xhci_hcsparams2) == 0x4);

union xhci_portsc {
    struct {
        uint32_t CCS : 1;
        uint32_t PED : 1;
        uint32_t : 1;
        uint32_t OCA : 1;
        uint32_t PR : 1;
        uint32_t PLS : 4;
        uint32_t PP : 1;
        uint32_t port_speed : 4;
        uint32_t PIC : 2;
        uint32_t LWS : 1;
        uint32_t CSC : 1;
        uint32_t PEC : 1;
        uint32_t WRC : 1;
        uint32_t OCC : 1;
        uint32_t PRC : 1;
        uint32_t PLC : 1;
        uint32_t CEC : 1;
        uint32_t CAS : 1;
        uint32_t WCE : 1;
        uint32_t WDE : 1;
        uint32_t WOE : 1;
        uint32_t : 2;
        uint32_t DR : 1;
        uint32_t WPR : 1;
    };
    uint32_t value;
} __attribute((packed));

static_assert(sizeof(xhci_portsc) == sizeof(uint32_t));
