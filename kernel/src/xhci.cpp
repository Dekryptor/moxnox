// SPDX-License-Identifier: GPL-2.0-only
module;

#include <atomic>
#include <stdint.h>
#include <stdlib.h>
#include <string_view>
#include <span>
#include <array>

export module xhci;

import nt;
import pci;
import usb;
import rwsem;
import object_ref;
import list;
import spinlock;
import page_cache;
import config;

using namespace std;

export class usb_device;

struct xhci_request;

const uint64_t USB_TIMEOUT = 5 * SYSTEM_TIME_RESOLUTION; // 5 seconds

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

const uint32_t XHCI_HCCPARAMS1_XECP = 0xffff0000;
const size_t XHCI_HCCPARAMS1_XECP_SHIFT = 16;
const uint32_t XHCI_HCCPARAMS1_CSZ = 0x00000004;

struct xhci_port_regs {
    uint32_t PORTSC;
    uint32_t PORTPMSC;
    uint32_t PORTLI;
    uint32_t PORTHLPMC;
};

static_assert(sizeof(xhci_port_regs) == 0x10);

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
};

static_assert(sizeof(xhci_op_regs) == 0x1400);

const uint32_t XHCI_USBCMD_RS = 0x00000001;
const uint32_t XHCI_USBCMD_HCRST = 0x00000002;
const uint32_t XHCI_USBCMD_INTE = 0x00000004;

struct xhci_int_regs {
    uint32_t IMAN;
    uint32_t IMOD;
    uint32_t ERSTSZ;
    uint32_t reserved;
    uint64_t ERSTBA;
    uint64_t ERDP;
} __attribute((packed));

static_assert(sizeof(xhci_int_regs) == 0x20);

const uint32_t XHCI_IMAN_IP = 0x00000001;
const uint32_t XHCI_IMAN_IE = 0x00000002;

const uint32_t XHCI_ERDP_EHB = 0x00000008;

const size_t MAX_INT_REGS = 1024;

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

struct xhci_trb_stop_endpoint {
    uint32_t reserved1[3];
    uint16_t cycle : 1;
    uint16_t reserved2 : 9;
    uint16_t trb_type : 6;
    uint8_t endpoint_id : 5;
    uint8_t sp : 1;
    uint8_t reserved3 : 2;
    uint8_t slot_id;
} __attribute((packed));

static_assert(sizeof(xhci_trb_stop_endpoint) == sizeof(xhci_trb_common));

struct xhci_trb_reset_endpoint {
    uint32_t reserved1[3];
    uint16_t cycle : 1;
    uint16_t reserved2 : 9;
    uint16_t trb_type : 6;
    uint8_t endpoint_id : 5;
    uint8_t reserved3 : 3;
    uint8_t slot_id;
} __attribute((packed));

static_assert(sizeof(xhci_trb_reset_endpoint) == sizeof(xhci_trb_common));

struct xhci_trb_set_tr_dequeue_pointer {
    uint64_t new_tr_dequeue_pointer;
    uint16_t stream_id;
    uint16_t reserved1;
    uint16_t cycle : 1;
    uint16_t reserved2 : 9;
    uint16_t trb_type : 6;
    uint8_t endpoint_id : 5;
    uint8_t reserved3 : 3;
    uint8_t slot_id;
} __attribute((packed));

static_assert(sizeof(xhci_trb_set_tr_dequeue_pointer) == sizeof(xhci_trb_common));

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

const uint32_t XHCI_EP_DCS = 0x00000001;

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

enum class xhci_portsc_pls : uint32_t {
    U0 = 0,
    U1 = 1,
    U2 = 2,
    U3 = 3,
    DISABLED = 4,
    RX_DETECT = 5,
    INACTIVE = 6,
    POLLING = 7,
    RECOVERY = 8,
    HOT_RESET = 9,
    COMPILANCE_MODE = 10,
    TEST_MODE = 11,
    RESUME = 15,
};

union xhci_portsc {
    struct {
        uint32_t CCS : 1;
        uint32_t PED : 1;
        uint32_t : 1;
        uint32_t OCA : 1;
        uint32_t PR : 1;
        enum xhci_portsc_pls PLS : 4;
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

export class xhci_queue {
public:
    xhci_queue(usb_device* dev) : dev(dev) { }

    ~xhci_queue();
    void initialize(uint16_t size, volatile uint32_t* doorbell,
                    uint32_t doorbell_val);
    void submit(xhci_request& req, bool ring_bell);

    template<size_t N>
    nt_status submit_multiple(array<xhci_request*, N>& req, uint64_t timeout);

    bool control_transfer(span<uint8_t> buf, uint8_t bmRequestType,
                          enum usb_request bRequest, uint16_t wValue,
                          uint16_t wIndex, uint64_t timeout = USB_TIMEOUT);
    size_t normal_transfer(span<uint8_t> buf);
    size_t normal_transfer_dio(pfn_iterator& it, uint64_t num_pages);
    void isr_transfer_event(const xhci_trb_transfer_event& ev);
    void break_waits();

    xhci_trb_common* ring = nullptr;
    bool initialized = false;
    uint16_t size;
    xhci_request** reqs = nullptr;
    bool dir;

private:
    unsigned int submit_holding_lock(xhci_request& req, bool ring_bell);
    void recover_ep0_after_timeout();
    void reset_ep0();
    TRB_CODE dequeue_pointer_ep0();

    spinlock submit_lock;
    unsigned int index = 0;
    bool cycle_bit = true;
    volatile uint32_t* doorbell;
    uint32_t doorbell_val;
    usb_device* dev;
};

export class xhci;

export class usb_hub {
public:
    usb_hub(xhci& controller, usb_device* device, unsigned int depth) : depth(depth), device(device), controller(controller) { }

    virtual ~usb_hub() {
        if (children)
            free(children);
    }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    void device_found(uint8_t slot, uint8_t port, enum usb_speed_class speed_class,
                      uint8_t port_speed);
    void device_removed(uint8_t port);

    unsigned int depth;
    usb_device* device;
    bool multi_tt = false;
    uint8_t num_ports;
    usb_device** children = nullptr;

private:
    xhci& controller;
    atomic<unsigned int> refcount = 1;
};

export class usb_device {
public:
    usb_device(xhci& controller, usb_hub& parent, void* devctx,
               uint8_t slot, uint8_t speed, uint8_t port,
               enum usb_speed_class speed_class);
    ~usb_device();
    bool get_descriptor(span<uint8_t> buf, enum usb_descriptor desc_type,
                        uint8_t desc_index);
    bool get_descriptor_class(span<uint8_t> buf, enum usb_descriptor desc_type,
                              uint8_t desc_index);
    bool get_descriptor_interface(span<uint8_t> buf, enum usb_descriptor desc_type,
                                  uint8_t desc_index, uint16_t int_num);
    bool set_configuration(uint8_t configuration_value);
    bool set_feature(uint8_t bmRequestType, uint8_t feature, uint16_t index);
    bool clear_feature(uint8_t bmRequestType, uint8_t feature, uint16_t index);
    bool get_status(uint8_t bmRequestType, span<uint8_t> buf, uint16_t wValue,
                    uint16_t wIndex);
    xhci_queue* configure_endpoint(const usb_endpoint_descriptor& ep_desc,
                                   uint8_t num_ports = 0, bool multi_tt = false,
                                   uint8_t tt_think_time = 0);
    void isr_transfer_event(const xhci_trb_transfer_event& ev);
    uint8_t find_root_port();
    pair<usb_device*, uint8_t> find_tt();
    bool set_interface(uint16_t interface, uint16_t alternate);
    void device_removed();

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    list_entry le;
    bool initialized = false;
    xhci_queue ep0_queue{this};
    uint8_t slot;
    usb_device_descriptor device_descriptor;
    list_entry endpoints = { &endpoints, &endpoints };
    xhci& controller;
    uint8_t speed;
    uint8_t port;
    usb_hub& parent;
    enum usb_speed_class speed_class;
    list_entry configs = { &configs, &configs };
    bool dead = false;
    void* removal_callback_ctx;
    void (*removal_callback)(void*) = nullptr;

private:
    bool change_ep0_max_packet_size(uint16_t max_packet_size);
    uint32_t find_route_string();

    void* devctx;
    void* input_context = nullptr;
    atomic<unsigned int> refcount = 1;
    rwsem endpoint_mutex;
    spinlock endpoint_spinlock;
};

export struct usb_endpoint {
    usb_endpoint(usb_endpoint_descriptor& desc) : desc(desc) { }

    list_entry le;
    usb_endpoint_descriptor& desc;
};

export struct usb_interface {
    usb_interface(usb_interface_descriptor& desc) : desc(desc) { }
    ~usb_interface();

    list_entry le;
    usb_interface_descriptor& desc;
    list_entry endpoints = { &endpoints, &endpoints };
};

export struct usb_device_config {
    ~usb_device_config();

    list_entry le;
    list_entry interfaces = { &interfaces, &interfaces };
    size_t len;
    uint8_t data[1];
};

export struct usb_recognizer {
    list_entry le;
    enum usb_class_code interface_code;
    bool (*func)(usb_device&, const usb_device_config&, const usb_interface&);
};

export list_entry usb_interfaces = { &usb_interfaces, &usb_interfaces };

export void print_usb_config(span<const uint8_t> buf);
export uint8_t xhci_enable_slot(xhci& controller, uint8_t slot_type);
export uint8_t xhci_get_speed_num(xhci& controller, uint8_t port_id,
                                  uint32_t psi_num);
