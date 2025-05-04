module;

#include <utility>
#include "kernel.h"
#include "xhci.h"
#include "spinlock.h"

export module xhci;

import pci;
import print;
import format;
import mem;
import config;
import interrupts;
import event;
import timer;
import usb;
import rwsem;

using namespace std;

static const uint16_t EVENT_RING_SIZE = 64;
static const uint16_t COMMAND_RING_SIZE = 64;
static const uint16_t ENDPOINT_RING_SIZE = 64;

static const int64_t ONE_MS_TICKS = 10000;

struct xhci_request {
    xhci_trb_common trb;
    union {
        xhci_trb_command_completion comp;
        xhci_trb_transfer_event ev;
    };
    object_ref event;
};

struct port_status_change {
    list_entry le;
    uint8_t port_id;
};

struct xhci_port {
    uint16_t usb_revision;
    uint8_t slot;
};

class xhci;

export class xhci_queue {
public:
    void initialize(uint16_t size, volatile uint32_t* doorbell,
                    uint32_t doorbell_val);
    void submit(xhci_request& req, bool ring_bell);
    bool control_transfer(span<uint8_t> buf, uint8_t bmRequestType,
                          enum usb_request bRequest, uint16_t wValue,
                          uint16_t wIndex);
    bool normal_transfer(span<uint8_t> buf);
    void isr_transfer_event(const xhci_trb_transfer_event& ev);
    ~xhci_queue();

    xhci_trb_common* ring = nullptr;
    uint64_t ring_pa;
    bool initialized = false;
    uint16_t size;
    xhci_request** reqs = nullptr;

private:
    spinlock submit_lock;
    unsigned int index = 0;
    bool cycle_bit = true;
    volatile uint32_t* doorbell;
    uint32_t doorbell_val;
};

class xhci {
public:
    xhci(pci_device& dev);
    ~xhci();
    void init_ports();
    void isr();
    void port_status_change_thread();
    uint8_t enable_slot(uint8_t slot_type);
    pair<uint32_t, uint16_t> get_speed(uint8_t port_id, uint8_t speed);
    void device_found(uint8_t slot, uint8_t root_port, uint32_t route_string,
                      uint16_t usb_revision, uint32_t psi_num,
                      uint8_t port_speed, unsigned int depth);

    bool initialized = false;
    pci_device& dev;
    uint32_t dboff;
    uint64_t* dcbaa = nullptr;
    xhci_queue command_queue;
    spinlock devices_lock;
    list_entry devices = { &devices, &devices };
    bool large_contexts;

private:
    void isr_command_completion(xhci_trb_command_completion& comp);
    void isr_port_status_change(xhci_trb_port_status_change& psc);
    void port_status_changed(uint8_t port_id);
    uint8_t get_port_slot_type(uint8_t port_id);
    bool reset_port(uint8_t port_id);
    bool init_scratchpad();
    uint16_t get_port_usb_revision(uint8_t port_id);

    uint8_t num_slots, num_ports;
    xhci_trb_common* event_ring = nullptr;
    uint64_t event_ring_pa;
    unsigned int event_ring_offset = 0;
    bool event_ring_cycle = true;
    xhci_event_ring_segment* seg_table = nullptr;
    uint8_t caplength;
    uint32_t rtsoff;
    object_ref port_status_change_event;
    object_ref port_status_change_thread_obj;
    spinlock port_status_change_lock;
    list_entry port_status_change_list = { &port_status_change_list, &port_status_change_list };
    xhci_port* ports = nullptr;
    uint64_t* scratchpad_array = nullptr;
    size_t scratchpad_array_size;
};

struct device_endpoint {
    list_entry le;
    uint8_t num;
    bool dir;
    xhci_queue queue;
};

enum class usb_speed_class {
    unknown,
    low_speed,
    high_speed,
    full_speed,
    super_speed,
    super_speed_plus
};

export class usb_device {
public:
    usb_device(xhci& controller, void* devctx,
               uint8_t slot, uint8_t speed, uint8_t root_port,
               uint32_t route_string, uint16_t usb_revision, xhci_psi& psi,
               unsigned int depth);
    ~usb_device();
    bool get_descriptor(span<uint8_t> buf, enum usb_descriptor desc_type,
                        uint8_t desc_index);
    bool get_descriptor_class(span<uint8_t> buf, enum usb_descriptor desc_type,
                              uint8_t desc_index);
    bool set_configuration(uint8_t configuration_value);
    bool set_feature(uint8_t bmRequestType, uint8_t feature, uint16_t index);
    bool clear_feature(uint8_t bmRequestType, uint8_t feature, uint16_t index);
    bool get_status(uint8_t bmRequestType, span<uint8_t> buf, uint16_t wValue,
                    uint16_t wIndex);
    xhci_queue* add_endpoint(const usb_endpoint_descriptor& ep_desc);
    void isr_transfer_event(const xhci_trb_transfer_event& ev);

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    list_entry le;
    bool initialized = false;
    xhci_queue ep0_queue;
    uint8_t slot;
    usb_device_descriptor device_descriptor;
    void* config = nullptr;
    size_t config_size;
    list_entry endpoints = { &endpoints, &endpoints };
    xhci& controller;
    uint8_t speed;
    uint8_t root_port;
    uint32_t route_string;
    unsigned int depth;

private:
    bool change_ep0_max_packet_size(uint16_t max_packet_size);

    void* devctx;
    atomic<unsigned int> refcount = 1;
    rwsem endpoint_mutex;
    spinlock endpoint_spinlock;
    enum usb_speed_class speed_class;
};

export struct usb_recognizer {
    list_entry le;
    enum usb_class_code interface_code;
    bool (*func)(usb_device&, uint8_t, bool);
};

export list_entry usb_interfaces = { &usb_interfaces, &usb_interfaces };

static void write_volatile64(volatile uint64_t* addr, uint64_t val) {
    *(volatile uint32_t*)addr = val & 0xffffffff;
    *((volatile uint32_t*)addr + 1) = (uint32_t)(val >> 32);
}

static void xhci_isr_func(void* ctx) {
    ((xhci*)ctx)->isr();
}

xhci_queue::~xhci_queue() {
    if (ring) {
        auto size_bytes = (size + 1) * sizeof(xhci_trb_common);

        uint32_t num_pages = bytes_to_pages(size_bytes);

        auto va = (uintptr_t)ring;

        for (unsigned int i = 0; i < num_pages; i++) {
            unmap_page(va + (i << PAGE_SHIFT));
        }

        free_va(va, num_pages);

        free_folio(ring_pa);
    }

    if (reqs)
        free(reqs);
}

void xhci_queue::initialize(uint16_t size, volatile uint32_t* doorbell,
                            uint32_t doorbell_val) {
    auto size_bytes = (size + 1) * sizeof(xhci_trb_common);

    uint32_t num_pages = bytes_to_pages(size_bytes);

    ring_pa = allocate_folio(bytes_to_folio_order(size_bytes));
    if (!ring_pa) {
        print("allocate folio failed\n");
        return;
    }

    auto va = allocate_va(num_pages);
    if (!va) {
        print("allocate_va failed\n");
        free_folio(ring_pa);
        return;
    }

    for (unsigned int i = 0; i < num_pages; i++) {
        if (!map_page(ring_pa + (i << PAGE_SHIFT), va + (i << PAGE_SHIFT), mapping_type::no_cache)) {
            print("map_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                unmap_page(va + (j << PAGE_SHIFT));
            }

            free_va(va, num_pages);
            free_folio(ring_pa);

            return;
        }
    }

    ring = (xhci_trb_common*)va;

    memset(ring, 0, size_bytes);

    reqs = (xhci_request**)malloc(sizeof(xhci_request*) * size);
    if (!reqs) {
        print("malloc failed\n");
        return;
    }

    auto& cmd_link = *reinterpret_cast<xhci_trb_link*>(&ring[size]);
    cmd_link.ring_segment_pointer = ring_pa;
    cmd_link.toggle_cycle = 1;
    cmd_link.trb_type = (uint16_t)xhci_trb_type::link;

    this->size = size;
    this->doorbell = doorbell;
    this->doorbell_val = doorbell_val;

    initialized = true;
}

static void copy_by_uint32(volatile void* src, volatile void* dest, size_t size) {
    auto src2 = (volatile uint32_t*)src;
    auto dest2 = (volatile uint32_t*)dest;

    while (size >= sizeof(uint32_t)) {
        *dest2 = *src2;
        src2++;
        dest2++;
        size -= sizeof(uint32_t);
    }
}

bool xhci::init_scratchpad() {
    volatile auto& cap_regs = *(xhci_cap_regs*)dev.bar[0].data();
    volatile auto& op_regs = *(xhci_op_regs*)((uint8_t*)&cap_regs + cap_regs.CAPLENGTH);

    xhci_hcsparams2 hcsparams2;

    copy_by_uint32(&cap_regs.HCSPARAMS2, &hcsparams2, sizeof(hcsparams2));

    uint16_t max_scratchpad_buffers = (uint16_t)(hcsparams2.max_scratchpad_buffers_high << 5) | hcsparams2.max_scratchpad_buffers_low;

    if (max_scratchpad_buffers == 0)
        return true;

    uint32_t op_pagesize_bits = op_regs.PAGESIZE;

    op_pagesize_bits &= 0xffff; // ignore reserved bits

    if (op_pagesize_bits == 0) {
        print("xhci::init_scratchpad: PAGESIZE was 0\n");
        return false;
    }

    uint8_t scratchpad_order = bytes_to_folio_order(1 << (countr_zero(op_pagesize_bits) + 12));

    scratchpad_array_size = max_scratchpad_buffers;

    scratchpad_array = (uint64_t*)malloc(sizeof(uint64_t) * scratchpad_array_size);
    if (!scratchpad_array) {
        print("malloc failed\n");
        return false;
    }

    for (unsigned int i = 0; i < max_scratchpad_buffers; i++) {
        auto buf = allocate_folio(scratchpad_order);
        if (!buf) {
            print("allocate_folio failed\n");

            for (unsigned int j = 0; j < i; j++) {
                free_folio(scratchpad_array[j]);
            }

            free(scratchpad_array);
            scratchpad_array = nullptr;

            return false;
        }

        memset(phys_address(buf), 0, 1 << (PAGE_SHIFT + scratchpad_order));

        scratchpad_array[i] = buf;
    }

    return true;
}

xhci::xhci(pci_device& dev) : dev(dev) {
    dev.header->command |= PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY;

    if (!dev.msix && dev.msi.empty()) {
        print("init_xhci: no MSI or MSI-X capability\n");
        return;
    }

    if (dev.bar[0].size() < sizeof(xhci_cap_regs)) {
        print("init_xhci: BAR0 was {} bytes, expected at least {}\n",
              dev.bar[0].size(), sizeof(xhci_cap_regs));
        return;
    }

    volatile auto& cap_regs = *(xhci_cap_regs*)dev.bar[0].data();

    if (cap_regs.CAPLENGTH < 0x20) {
        print("init_xhci: CAPLENGTH was {}, expected at least {}\n",
              cap_regs.CAPLENGTH, 0x20);
        return;
    }

    large_contexts = cap_regs.HCCPARAMS1 & XHCI_HCCPARAMS1_CSZ;

    xhci_hcsparams1 hcsparams1;

    copy_by_uint32(&cap_regs.HCSPARAMS1, &hcsparams1, sizeof(hcsparams1));

    num_slots = hcsparams1.max_slots;
    num_ports = hcsparams1.max_ports;

    if (num_ports == 0) {
        print("xhci::xhci: num_ports was 0\n");
        return;
    }

    ports = (xhci_port*)malloc(sizeof(xhci_port) * num_ports);
    if (!ports) {
        print("malloc failed\n");
        return;
    }

    memset(ports, 0, sizeof(xhci_port) * num_ports);

    if (dev.bar[0].size() < cap_regs.CAPLENGTH + offsetof(xhci_op_regs, ports) + (num_ports * sizeof(xhci_port_regs))) {
        print("init_xhci: BAR0 was {} bytes, expected at least {}\n",
              dev.bar[0].size(), cap_regs.CAPLENGTH + sizeof(xhci_op_regs) + (num_ports * sizeof(xhci_port_regs)));
        return;
    }

    if (!init_scratchpad()) {
        print("xhci::init_scratchpad failed\n");
        return;
    }

    volatile auto& op_regs = *(xhci_op_regs*)((uint8_t*)&cap_regs + cap_regs.CAPLENGTH);

    {
        auto tim_ref = create_timer(true);
        if (!tim_ref) {
            print("xhci::xhci: create_timer failed\n");
            return;
        }

        auto& tim = static_cast<timer&>(tim_ref.get());

        auto status = tim.set(-ONE_MS_TICKS, nullptr, nullptr, false, 1, nullptr);
        if (!success(status)) {
            print("xhci::xhci: timer::set returned {}\n", status);
            return;
        }

        op_regs.USBCMD &= ~XHCI_USBCMD_RS; // unset run bit
        for (unsigned int i = 0; i < 16; i++) { // specs say we should wait up to 16ms
            if (op_regs.USBSTS & 1) // halted
                break;

            wait_for_object(tim, true); // wait 1ms
        }
    }

    op_regs.USBCMD = XHCI_USBCMD_HCRST; // reset

    bool reset_done = false;

    {
        auto tim_ref = create_timer(true);
        if (!tim_ref) {
            print("xhci::xhci: create_timer failed\n");
            return;
        }

        auto& tim = static_cast<timer&>(tim_ref.get());

        auto status = tim.set(-ONE_MS_TICKS, nullptr, nullptr, false, 1, nullptr);
        if (!success(status)) {
            print("xhci::xhci: timer::set returned {}\n", status);
            return;
        }

        // wait for reset to complete
        for (unsigned int i = 0; i < 500; i++) {
            if (!(op_regs.USBCMD & XHCI_USBCMD_HCRST)) {
                reset_done = true;
                break;
            }

            wait_for_object(tim, true); // wait 1ms
        }
    }

    if (!reset_done) {
        print("init_xhci: timed out resetting controller\n");
        return;
    }

    for (uint8_t i = 0; i < num_ports; i++) {
        ports[i].usb_revision = get_port_usb_revision(i + 1);
    }

    print("init_xhci: num_slots = {}, num_ports = {}\n", num_slots, num_ports);

    dcbaa = (uint64_t*)malloc((num_slots + 1) * sizeof(uint64_t));
    if (!dcbaa) {
        print("malloc failed\n");
        return;
    }

    memset(dcbaa, 0, (num_slots + 1) * sizeof(uint64_t));
    dcbaa[0] = va_to_pa_phys(scratchpad_array);

    write_volatile64(&op_regs.DCBAAP, va_to_pa_phys(dcbaa));

    dboff = cap_regs.DBOFF;

    command_queue.initialize(COMMAND_RING_SIZE, (uint32_t*)(dev.bar[0].data() + dboff), 0);

    write_volatile64(&op_regs.CRCR, command_queue.ring_pa | 1);

    op_regs.CONFIG = (op_regs.CONFIG & ~(uint32_t)0xff) | num_slots;

    if (dev.bar[0].size() < cap_regs.RTSOFF + offsetof(xhci_run_regs, int_regs) + sizeof(xhci_int_regs)) {
        print("init_xhci: BAR0 was {} bytes, expected at least {}\n",
              dev.bar[0].size(), cap_regs.RTSOFF + offsetof(xhci_run_regs, int_regs) + sizeof(xhci_int_regs));
        return;
    }

    volatile auto& run_regs = *(xhci_run_regs*)((uint8_t*)&cap_regs + cap_regs.RTSOFF);

    size_t event_ring_bytes = EVENT_RING_SIZE * sizeof(xhci_trb_common);
    uint32_t event_ring_num_pages = bytes_to_pages(event_ring_bytes);

    event_ring_pa = allocate_folio(bytes_to_folio_order(event_ring_bytes));
    if (!event_ring_pa) {
        print("allocate folio failed\n");
        return;
    }

    auto va = allocate_va(event_ring_num_pages);
    if (!va) {
        print("allocate_va failed\n");
        free_folio(event_ring_pa);
        return;
    }

    for (unsigned int i = 0; i < event_ring_num_pages; i++) {
        if (!map_page(event_ring_pa + (i << PAGE_SHIFT), va + (i << PAGE_SHIFT), mapping_type::no_cache)) {
            print("map_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                unmap_page(va + (j << PAGE_SHIFT));
            }

            free_va(va, event_ring_num_pages);
            free_folio(event_ring_pa);

            return;
        }
    }

    event_ring = (xhci_trb_common*)va;
    memset(event_ring, 0, event_ring_bytes);

    seg_table = (xhci_event_ring_segment*)malloc_aligned(sizeof(xhci_event_ring_segment), 64);
    if (!seg_table) {
        print("malloc failed\n");
        return;
    }

    memset(seg_table, 0, sizeof(xhci_event_ring_segment));

    seg_table->base_address = event_ring_pa;
    seg_table->size = EVENT_RING_SIZE;

    volatile auto& int_reg = run_regs.int_regs[0];

    int_reg.ERSTSZ |= (int_reg.ERSTSZ & 0xffff0000) | 1;
    write_volatile64(&int_reg.ERDP, event_ring_pa);
    write_volatile64(&int_reg.ERSTBA, va_to_pa_phys(seg_table));

    port_status_change_event = create_event(true);

    // FIXME - stop thread if xhci object goes away(?)
    port_status_change_thread_obj = add_kernel_thread([](void* ctx) {
        ((xhci*)ctx)->port_status_change_thread();
    }, this);

    // FIXME - free interrupt number in destructor
    auto int_num = add_isr(xhci_isr_func, this);
    if (int_num == 0) {
        print("add_isr failed\n");
        return;
    }

    if (dev.msix) {
        // enable MSI-X
        dev.msix->message_control |= 0x8000;

        // set up interrupt
        auto& vec = *(volatile msix_entry*)(dev.bar[dev.msix->bir].data() + (dev.msix->table_offset << 3));

        vec.message_data = int_num;
        vec.message_address_high = 0;
        vec.message_address_low = apic_msi_address(0, false, false); // FIXME - spread out amongst processors
        vec.vector_control &= ~(uint32_t)1; // unmask
    } else {
        auto& msi = *(volatile pci_msi*)dev.msi.data();

        msi.message_address_low = apic_msi_address(0, false, false); // FIXME - spread out amongst processors

        if (msi.message_control & 0x80) { // 64-bit
            msi.v64.message_address_high = 0;
            msi.v64.message_data = int_num;
        } else
            msi.v32.message_data = int_num;

        msi.message_control = msi.message_control | 1; // enable
    }

    int_reg.IMAN |= XHCI_IMAN_IE;

    op_regs.USBCMD |= XHCI_USBCMD_INTE | XHCI_USBCMD_RS;

    caplength = cap_regs.CAPLENGTH;
    rtsoff = cap_regs.RTSOFF;

    initialized = true;
}

xhci::~xhci() {
    if (dcbaa)
        free(dcbaa);

    if (event_ring) {
        auto size_bytes = EVENT_RING_SIZE * sizeof(xhci_trb_common);

        uint32_t num_pages = bytes_to_pages(size_bytes);

        auto va = (uintptr_t)event_ring;

        for (unsigned int i = 0; i < num_pages; i++) {
            unmap_page(va + (i << PAGE_SHIFT));
        }

        free_va(va, num_pages);

        free_folio(event_ring_pa);
    }

    if (seg_table)
        free(seg_table);

    if (ports)
        free(ports);

    if (scratchpad_array) {
        for (size_t i = 0; i < scratchpad_array_size; i++) {
            free_folio(scratchpad_array[i]);
        }
        free(scratchpad_array);
    }
}

bool xhci::reset_port(uint8_t port_id) {
    volatile auto& cap_regs = *(xhci_cap_regs*)dev.bar[0].data();
    volatile auto& op_regs = *(xhci_op_regs*)((uint8_t*)&cap_regs + cap_regs.CAPLENGTH);
    volatile auto& p = *&op_regs.ports[port_id - 1];
    auto portsc = p.PORTSC;

    // reset port
    portsc &= XHCI_PORTSC_PP | XHCI_PORTSC_PIC;
    portsc |= XHCI_PORTSC_PR;

    p.PORTSC = portsc;

    bool port_enabled = false;

    auto tim_ref = create_timer(true);
    if (!tim_ref) {
        print("xhci::reset_port: create_timer failed\n");
        return false;
    }

    auto& tim = static_cast<timer&>(tim_ref.get());

    auto status = tim.set(-ONE_MS_TICKS, nullptr, nullptr, false, 1, nullptr);
    if (!success(status)) {
        print("xhci::reset_port: timer::set returned {}\n", status);
        return false;
    }

    for (unsigned int j = 0; j < 500; j++) {
        if (p.PORTSC & XHCI_PORTSC_PED) {
            port_enabled = true;
            break;
        }

        wait_for_object(tim, true); // wait 1ms
    }

    if (!port_enabled)
        print("xhci::reset_port: timed out waiting for port {} to be enabled\n", port_id);

    return port_enabled;
}

void xhci::init_ports() {
    volatile auto& cap_regs = *(xhci_cap_regs*)dev.bar[0].data();
    volatile auto& op_regs = *(xhci_op_regs*)((uint8_t*)&cap_regs + cap_regs.CAPLENGTH);

    for (uint8_t i = 0; i < num_ports; i++) {
        auto portsc = op_regs.ports[i].PORTSC;

        if (!(portsc & XHCI_PORTSC_CCS)) // ignore if no device connected
            continue;

        reset_port(i + 1);
    }
}

void xhci::isr_command_completion(xhci_trb_command_completion& comp) {
    if (comp.command_trb_pointer < command_queue.ring_pa || comp.command_trb_pointer >= command_queue.ring_pa + (command_queue.size * sizeof(xhci_trb_common)))
        return;

    size_t index = (size_t)(comp.command_trb_pointer - command_queue.ring_pa) / sizeof(xhci_trb_common);
    auto& req = *command_queue.reqs[index];

    memcpy(&req.comp, &comp, sizeof(comp));

    set_event(static_cast<event&>(req.event.get()));
}

template<>
consteval bool format_spec_valid<enum TRB_CODE>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, enum TRB_CODE arg) {
    switch (arg) {
        case TRB_CODE::INVALID:
            return do_format_arg(sp, "", "INVALID");

        case TRB_CODE::SUCCESS:
            return do_format_arg(sp, "", "SUCCESS");

        case TRB_CODE::DATA_BUFFER_ERROR:
            return do_format_arg(sp, "", "DATA_BUFFER_ERROR");

        case TRB_CODE::BABBLE_DETECTED:
            return do_format_arg(sp, "", "BABBLE_DETECTED");

        case TRB_CODE::USB_TRANSACTION_ERROR:
            return do_format_arg(sp, "", "USB_TRANSACTION_ERROR");

        case TRB_CODE::TRB_ERROR:
            return do_format_arg(sp, "", "TRB_ERROR");

        case TRB_CODE::STALL_ERROR:
            return do_format_arg(sp, "", "STALL_ERROR");

        case TRB_CODE::RESOURCE_ERROR:
            return do_format_arg(sp, "", "RESOURCE_ERROR");

        case TRB_CODE::BANDWIDTH_ERROR:
            return do_format_arg(sp, "", "BANDWIDTH_ERROR");

        case TRB_CODE::NO_SLOTS_AVAILABLE_ERROR:
            return do_format_arg(sp, "", "NO_SLOTS_AVAILABLE_ERROR");

        case TRB_CODE::INVALID_STREAM_TYPE:
            return do_format_arg(sp, "", "INVALID_STREAM_TYPE");

        case TRB_CODE::SLOT_NOT_ENABLED:
            return do_format_arg(sp, "", "SLOT_NOT_ENABLED");

        case TRB_CODE::ENDPOINT_NOT_ENABLED:
            return do_format_arg(sp, "", "ENDPOINT_NOT_ENABLED");

        case TRB_CODE::SHORT_PACKET:
            return do_format_arg(sp, "", "SHORT_PACKET");

        case TRB_CODE::RING_UNDERRUN:
            return do_format_arg(sp, "", "RING_UNDERRUN");

        case TRB_CODE::RING_OVERRUN:
            return do_format_arg(sp, "", "RING_OVERRUN");

        case TRB_CODE::VF_EVENT_RING_FULL:
            return do_format_arg(sp, "", "VF_EVENT_RING_FULL");

        case TRB_CODE::PARAMETER_ERROR:
            return do_format_arg(sp, "", "PARAMETER_ERROR");

        case TRB_CODE::BANDWIDTH_OVERRUN:
            return do_format_arg(sp, "", "BANDWIDTH_OVERRUN");

        case TRB_CODE::CONTEXT_STATE_ERROR:
            return do_format_arg(sp, "", "CONTEXT_STATE_ERROR");

        case TRB_CODE::NO_PINT_RESPONSE:
            return do_format_arg(sp, "", "NO_PINT_RESPONSE");

        case TRB_CODE::EVENT_RING_FULL:
            return do_format_arg(sp, "", "EVENT_RING_FULL");

        case TRB_CODE::INCOMPATIBLE_DEVICE:
            return do_format_arg(sp, "", "INCOMPATIBLE_DEVICE");

        case TRB_CODE::MISSED_SERVICE:
            return do_format_arg(sp, "", "MISSED_SERVICE");

        case TRB_CODE::COMMAND_RING_STOPPED:
            return do_format_arg(sp, "", "COMMAND_RING_STOPPED");

        case TRB_CODE::COMMAND_ABORTED:
            return do_format_arg(sp, "", "COMMAND_ABORTED");

        case TRB_CODE::STOPPED:
            return do_format_arg(sp, "", "STOPPED");

        case TRB_CODE::STOPPED_LENGTH_INVALID:
            return do_format_arg(sp, "", "STOPPED_LENGTH_INVALID");

        case TRB_CODE::STOPPED_SHORT_PACKET:
            return do_format_arg(sp, "", "STOPPED_SHORT_PACKET");

        case TRB_CODE::MAX_EXIT_LATENCY_TOO_LARGE:
            return do_format_arg(sp, "", "MAX_EXIT_LATENCY_TOO_LARGE");

        case TRB_CODE::ISOCH_BUFFER_OVERRUN:
            return do_format_arg(sp, "", "ISOCH_BUFFER_OVERRUN");

        case TRB_CODE::EVENT_LOST:
            return do_format_arg(sp, "", "EVENT_LOST");

        case TRB_CODE::UNDEFINED_ERROR:
            return do_format_arg(sp, "", "UNDEFINED_ERROR");

        case TRB_CODE::INVALID_STREAM_ID:
            return do_format_arg(sp, "", "INVALID_STREAM_ID");

        case TRB_CODE::SECONDARY_BANDWIDTH_ERROR:
            return do_format_arg(sp, "", "SECONDARY_BANDWIDTH_ERROR");

        case TRB_CODE::SPLIT_TRANSACTION_ERROR:
            return do_format_arg(sp, "", "SPLIT_TRANSACTION_ERROR");

        default:
            return do_format_arg(sp, "", (uint32_t)arg);
    }
}

template<>
consteval bool format_spec_valid<enum usb_class_code>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, enum usb_class_code arg) {
    switch (arg) {
        case usb_class_code::per_interface:
            return do_format_arg(sp, "", "per_interface");

        case usb_class_code::audio:
            return do_format_arg(sp, "", "audio");

        case usb_class_code::communications:
            return do_format_arg(sp, "", "communications");

        case usb_class_code::hid:
            return do_format_arg(sp, "", "hid");

        case usb_class_code::physical:
            return do_format_arg(sp, "", "physical");

        case usb_class_code::image:
            return do_format_arg(sp, "", "image");

        case usb_class_code::printer:
            return do_format_arg(sp, "", "printer");

        case usb_class_code::mass_storage:
            return do_format_arg(sp, "", "mass_storage");

        case usb_class_code::hub:
            return do_format_arg(sp, "", "hub");

        case usb_class_code::cdc_data:
            return do_format_arg(sp, "", "cdc_data");

        case usb_class_code::smart_card:
            return do_format_arg(sp, "", "smart_card");

        case usb_class_code::content_security:
            return do_format_arg(sp, "", "content_security");

        case usb_class_code::video:
            return do_format_arg(sp, "", "video");

        case usb_class_code::personal_healthcare:
            return do_format_arg(sp, "", "personal_healthcare");

        case usb_class_code::audio_video_devices:
            return do_format_arg(sp, "", "audio_video_devices");

        case usb_class_code::billboard_device:
            return do_format_arg(sp, "", "billboard_device");

        case usb_class_code::type_c_bridge:
            return do_format_arg(sp, "", "type_c_bridge");

        case usb_class_code::bulk_display_protocol_device:
            return do_format_arg(sp, "", "bulk_display_protocol_device");

        case usb_class_code::mctp_over_usb_endpoint_device:
            return do_format_arg(sp, "", "mctp_over_usb_endpoint_device");

        case usb_class_code::i3c_device:
            return do_format_arg(sp, "", "i3c_device");

        case usb_class_code::diagonstic_device:
            return do_format_arg(sp, "", "diagonstic_device");

        case usb_class_code::wireless_controller:
            return do_format_arg(sp, "", "wireless_controller");

        case usb_class_code::miscellaneous:
            return do_format_arg(sp, "", "miscellaneous");

        case usb_class_code::application_specific:
            return do_format_arg(sp, "", "application_specific");

        case usb_class_code::vendor_specific:
            return do_format_arg(sp, "", "vendor_specific");

        default:
            return do_format_arg(sp, "", (uint32_t)arg);
    }
}

static string_view psie_to_string(uint8_t psie) {
    switch (psie) {
        case 0:
            return "bits/s";

        case 1:
            return "Kb/s";

        case 2:
            return "Mb/s";

        case 3:
            return "Gb/s";

        default:
            unreachable();
    }
}

static string_view speed_class_to_string(enum usb_speed_class sc) {
    switch (sc) {
        case usb_speed_class::unknown:
            return "unknown speed";

        case usb_speed_class::low_speed:
            return "Low Speed";

        case usb_speed_class::high_speed:
            return "High Speed";

        case usb_speed_class::full_speed:
            return "Full Speed";

        case usb_speed_class::super_speed:
            return "SuperSpeed";

        case usb_speed_class::super_speed_plus:
            return "SuperSpeed+";

        default:
            unreachable();
    }
}

usb_device::usb_device(xhci& controller, void* devctx, uint8_t slot,
                       uint8_t speed, uint8_t root_port, uint32_t route_string,
                       uint16_t usb_revision, xhci_psi& psi, unsigned int depth) : controller(controller), devctx(devctx) {
    this->slot = slot;
    this->speed = speed;
    this->root_port = root_port;
    this->route_string = route_string;
    this->depth = depth;

    speed_class = usb_speed_class::unknown;

    if (psi.psim == 12 && psi.psie == 2)
        speed_class = usb_speed_class::full_speed;
    else if (psi.psim == 1500 && psi.psie == 1)
        speed_class = usb_speed_class::low_speed;
    else if (psi.psim == 480 && psi.psie == 2)
        speed_class = usb_speed_class::high_speed;
    else if ((usb_revision & 0xff00) == 0x0300) {
        if (psi.lp == 0)
            speed_class = usb_speed_class::super_speed;
        else if (psi.lp == 1)
            speed_class = usb_speed_class::super_speed_plus;
    }

    print("route string {:x}, slot {}, speed {} {}, {}, USB {}.{}\n",
          route_string, slot, psi.psim,
          psie_to_string(psi.psie), speed_class_to_string(speed_class),
          usb_revision >> 8, (usb_revision & 0xff) >> 4);

    if (speed_class == usb_speed_class::unknown) {
        print("usb_device::usb_device: ignoring device with unknown speed class\n");
        return;
    }

    uint16_t max_packet_size;

    auto func = [&]<bool LARGE>() -> bool {
        auto context = new xhci_input_context<LARGE>;
        if (!context) {
            print("new failed\n");
            return false;
        }

        memset(context, 0, sizeof(*context));
        context->control.add_context = 3; // slot and EP0

        ep0_queue.initialize(ENDPOINT_RING_SIZE, (uint32_t*)(controller.dev.bar[0].data() + controller.dboff + (slot * sizeof(uint32_t))), 1);
        if (!ep0_queue.initialized) {
            print("xhci_queue::initialize failed\n");
            delete context;
            return false;
        }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        context->slot.speed = speed;
        context->slot.root_hub_port_number = root_port;
        context->slot.route_string = route_string;
#pragma GCC diagnostic pop
        // FIXME - multi_tt
        // FIXME - hub
        // FIXME - num_ports
        // FIXME - parent_hub_slot_id
        // FIXME - parent_port_number
        // FIXME - ttt
        context->slot.context_entries = 1;

        switch (speed_class) {
            case usb_speed_class::low_speed:
            case usb_speed_class::full_speed:
                max_packet_size = 8;
                break;

            case usb_speed_class::high_speed:
                max_packet_size = 64;
                break;

            case usb_speed_class::super_speed:
            case usb_speed_class::super_speed_plus:
                max_packet_size = 512;
                break;

            case usb_speed_class::unknown:
                unreachable();
                break;
        }

        context->ep[0].ep_type = (uint8_t)XHCI_EP_TYPE::CONTROL;
        context->ep[0].max_packet_size = max_packet_size;
        context->ep[0].tr_dequeue_pointer = ep0_queue.ring_pa | XHCI_EP_DCS;
        context->ep[0].average_trb_length = max_packet_size;
        context->ep[0].cerr = 3;

        // some devices need an address_device with BSR set first
        for (unsigned int i = 0; i < 2; i++) {
            xhci_request req;

            auto& s = *reinterpret_cast<xhci_trb_address_device*>(&req.trb);

            memset(&s, 0, sizeof(s));

            s.input_context_pointer = va_to_pa_phys(context);
            s.trb_type = (uint16_t)xhci_trb_type::address_device_command;
            s.slot_id = slot;
            s.bsr = i == 0 ? 1 : 0;

            controller.command_queue.submit(req, true);

            wait_for_object(req.event.get(), false);

            // FIXME - translate into nt_status?
            if ((enum TRB_CODE)req.comp.completion_code != TRB_CODE::SUCCESS) {
                print("usb_device::usb_device: controller returned error {} for address device\n", (enum TRB_CODE)req.comp.completion_code);
                delete context;
                return false;
            }
        }

        delete context;

        return true;
    };

    if (controller.large_contexts) {
        if (!func.template operator()<true>())
            return;
    } else {
        if (!func.template operator()<false>())
            return;
    }

    {
        lock_guard lock(controller.devices_lock);
        insert_tail_list(controller.devices, le);
    }

    // if Full Speed, update EP0 with actual max packet size
    if (speed_class == usb_speed_class::full_speed) {
        auto buf = (uint8_t*)malloc(8);
        if (!buf) {
            print("malloc failed\n");

            {
                lock_guard lock(controller.devices_lock);
                remove_entry_list(le);
            }

            return;
        }

        if (!get_descriptor(span(buf, 8), usb_descriptor::DEVICE, 0)) {
            print("usb_device::get_descriptor failed\n");
            free(buf);

            {
                lock_guard lock(controller.devices_lock);
                remove_entry_list(le);
            }

            return;
        }

        auto& dd = *(usb_device_descriptor*)buf;
        uint16_t actual_max_packet_size = dd.bMaxPacketSize0;
        free(buf);

        if (actual_max_packet_size != max_packet_size) {
            if (!change_ep0_max_packet_size(actual_max_packet_size)) {
                print("usb_device::change_ep0_max_packet_size failed\n");

                {
                    lock_guard lock(controller.devices_lock);
                    remove_entry_list(le);
                }

                return;
            }
        }
    }

    initialized = true;
}

usb_device::~usb_device() {
    // FIXME - disable slot

    controller.dcbaa[slot] = 0;

    if (controller.large_contexts)
        delete (xhci_device_context<true>*)devctx;
    else
        delete (xhci_device_context<false>*)devctx;

    if (initialized) {
        lock_guard lock(controller.devices_lock);
        remove_entry_list(le);
    }

    if (config) {
        if (config_size < PAGE_SIZE)
            free(config);
        else
            free_folio(phys_memory_pfn(config) << PAGE_SHIFT);
    }
}

bool usb_device::change_ep0_max_packet_size(uint16_t max_packet_size) {
    xhci_request req;

    // evaluate context command uses same TRB format as address device
    auto& s = *reinterpret_cast<xhci_trb_address_device*>(&req.trb);

    memset(&s, 0, sizeof(s));

    auto func = [&]<bool LARGE>() -> bool {
        auto context = new xhci_input_context<LARGE>;
        if (!context) {
            print("new failed\n");
            return false;
        }

        memset(context, 0, sizeof(*context));
        context->control.add_context = 3; // slot and EP0

        s.input_context_pointer = va_to_pa_phys(context);
        s.trb_type = (uint16_t)xhci_trb_type::evaluate_context_command;
        s.slot_id = slot;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        context->slot.speed = speed;
        context->slot.root_hub_port_number = root_port;
        context->slot.route_string = route_string;
#pragma GCC diagnostic pop
        context->slot.context_entries = 1;

        context->ep[0].ep_type = (uint8_t)XHCI_EP_TYPE::CONTROL;
        context->ep[0].max_packet_size = max_packet_size;
        context->ep[0].tr_dequeue_pointer = ep0_queue.ring_pa | XHCI_EP_DCS;
        context->ep[0].average_trb_length = max_packet_size;
        context->ep[0].cerr = 3;

        controller.command_queue.submit(req, true);

        wait_for_object(req.event.get(), false);

        delete context;

        return true;
    };

    if (controller.large_contexts) {
        if (!func.template operator()<true>())
            return false;
    } else {
        if (!func.template operator()<false>())
            return false;
    }

    if ((enum TRB_CODE)req.comp.completion_code != TRB_CODE::SUCCESS) {
        print("usb_device::change_ep0_max_packet_size: controller returned error {}\n", (enum TRB_CODE)req.comp.completion_code);
        return false;
    }

    return true;
}

static uint8_t find_interval(const usb_endpoint_descriptor& ep_desc,
                             enum usb_speed_class speed_class) {
    switch (ep_desc.transfer_type) {
        case USB_ENDPOINT_TRANSFER_TYPE_CONTROL:
        case USB_ENDPOINT_TRANSFER_TYPE_BULK:
            switch (speed_class) {
                case usb_speed_class::high_speed:
                    if (ep_desc.bInterval == 0)
                        return 0;
                    else
                        return (uint8_t)(bit_width(ep_desc.bInterval) - 1);

                default:
                    return 0;
            }
        break;

        case USB_ENDPOINT_TRANSFER_TYPE_INTERRUPT:
            switch (speed_class) {
                case usb_speed_class::low_speed:
                case usb_speed_class::full_speed:
                    if (ep_desc.bInterval == 0)
                        return 0;
                    else
                        return (uint8_t)(bit_width((unsigned int)(ep_desc.bInterval * 8)) - 1);

                default:
                    return (uint8_t)min(max(ep_desc.bInterval - 1, 0), 15);
            }
        break;

        case USB_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS:
            switch (speed_class) {
                case usb_speed_class::low_speed: // low-speed isochronous not allowed
                    return 0;

                case usb_speed_class::full_speed:
                    return (uint8_t)min(max(ep_desc.bInterval + 2, 3), 18);

                default:
                    return (uint8_t)min(max(ep_desc.bInterval - 1, 0), 15);
            }
        break;
    }

    return 0;
}

xhci_queue* usb_device::add_endpoint(const usb_endpoint_descriptor& ep_desc) {
    xhci_request req;

    auto& s = *reinterpret_cast<xhci_trb_configure_endpoint*>(&req.trb);

    memset(&s, 0, sizeof(s));

    device_endpoint* dep;

    auto func = [&]<bool LARGE>() {
        auto context = new xhci_input_context<LARGE>;
        if (!context) {
            print("new failed\n");
            return false;
        }

        unsigned int ep_index = 1 + ((ep_desc.endpoint_number - 1) * 2) + (ep_desc.direction ? 1 : 0);

        memset(context, 0, sizeof(*context));
        context->control.add_context = 1 | (1 << (ep_index + 1)); // slot, endpoint

        s.input_context_pointer = va_to_pa_phys(context);
        s.trb_type = (uint16_t)xhci_trb_type::configure_endpoint_command;
        s.slot_id = slot;

        dep = new device_endpoint;
        if (!dep) {
            print("new failed\n");
            delete context;
            return false;
        }

        dep->num = ep_desc.endpoint_number;
        dep->dir = ep_desc.direction;

        dep->queue.initialize(ENDPOINT_RING_SIZE, (uint32_t*)(controller.dev.bar[0].data() + controller.dboff + (slot * sizeof(uint32_t))),
                            ep_index + 1);
        if (!dep->queue.initialized) {
            print("xhci_queue::initialize failed\n");
            delete dep;
            delete context;
            return false;
        }

        {
            lock_guard lock(endpoint_mutex);

            {
                lock_guard lock2(endpoint_spinlock);

                for (auto le = endpoints.next; le != &endpoints; le = le->next) {
                    auto& e = containing_record<device_endpoint>(le);

                    if (e.num == dep->num && !!e.dir == !!dep->dir) {
                        print("usb_device::add_endpoint: endpoint {} {} already set up\n",
                            dep->num, dep->dir ? "IN" : "OUT");
                        delete dep;
                        delete context;
                        return false;
                    }
                }
            }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            context->slot.speed = speed;
            context->slot.context_entries = (uint8_t)(ep_index + 1);
            context->slot.root_hub_port_number = root_port;
            context->slot.route_string = route_string;
#pragma GCC diagnostic pop

            auto& ep = context->ep[ep_index];

            switch (ep_desc.transfer_type) {
                case USB_ENDPOINT_TRANSFER_TYPE_CONTROL:
                    ep.ep_type = (uint8_t)XHCI_EP_TYPE::CONTROL;
                    break;

                case USB_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS:
                    ep.ep_type = dep->dir ? (uint8_t)XHCI_EP_TYPE::ISOCH_IN : (uint8_t)XHCI_EP_TYPE::ISOCH_OUT;
                    break;

                case USB_ENDPOINT_TRANSFER_TYPE_BULK:
                    ep.ep_type = dep->dir ? (uint8_t)XHCI_EP_TYPE::BULK_IN : (uint8_t)XHCI_EP_TYPE::BULK_OUT;
                    break;

                case USB_ENDPOINT_TRANSFER_TYPE_INTERRUPT:
                    ep.ep_type = dep->dir ? (uint8_t)XHCI_EP_TYPE::INTERRUPT_IN : (uint8_t)XHCI_EP_TYPE::INTERRUPT_OUT;
                    break;
            }

            // FIXME - max_burst_size?
            // FIXME - max_esit_payload?

            ep.interval = find_interval(ep_desc, speed_class);
            ep.max_packet_size = ep_desc.wMaxPacketSize;
            ep.tr_dequeue_pointer = dep->queue.ring_pa | XHCI_EP_DCS;
            ep.average_trb_length = ep_desc.wMaxPacketSize;
            ep.cerr = 3;

            controller.command_queue.submit(req, true);

            wait_for_object(req.event.get(), false);

            if ((enum TRB_CODE)req.comp.completion_code == TRB_CODE::SUCCESS) {
                lock_guard lock2(endpoint_spinlock);
                insert_tail_list(endpoints, dep->le);
            }
        }

        delete context;

        return true;
    };

    if (controller.large_contexts) {
        if (!func.template operator()<true>())
            return nullptr;
    } else {
        if (!func.template operator()<false>())
            return nullptr;
    }

    // FIXME - translate into nt_status?
    if ((enum TRB_CODE)req.comp.completion_code != TRB_CODE::SUCCESS) {
        print("usb_device::add_endpoint: controller returned error {}\n", (enum TRB_CODE)req.comp.completion_code);
        return nullptr;
    }

    return &dep->queue;
}

bool xhci_queue::control_transfer(span<uint8_t> buf, uint8_t bmRequestType,
                                  enum usb_request bRequest, uint16_t wValue,
                                  uint16_t wIndex) {
    xhci_request req_setup, req_data, req_status;

    {
        auto& s = *reinterpret_cast<xhci_trb_setup_stage*>(&req_setup.trb);

        memset(&s, 0, sizeof(s));

        s.bmRequestType = bmRequestType;
        s.bRequest = (uint8_t)bRequest;
        s.wValue = wValue;
        s.wIndex = wIndex;
        s.wLength = (uint16_t)buf.size();
        s.trb_transfer_length = 8;
        s.idt = 1;
        s.trb_type = (uint32_t)xhci_trb_type::setup_stage;

        if (buf.empty())
            s.transfer_type = 0; // no data
        else if (bmRequestType & USB_REQUEST_TYPE_DEVICE_TO_HOST)
            s.transfer_type = 3; // IN
        else
            s.transfer_type = 2; // OUT

        submit(req_setup, false);
    }

    if (!buf.empty()) {
        auto& s = *reinterpret_cast<xhci_trb_data_stage*>(&req_data.trb);

        memset(&s, 0, sizeof(s));

        s.data_buffer = va_to_pa_phys(buf.data());
        s.trb_transfer_length = (uint16_t)buf.size();
        s.trb_type = (uint16_t)xhci_trb_type::data_stage;
        s.dir = !!(bmRequestType & USB_REQUEST_TYPE_DEVICE_TO_HOST);

        submit(req_data, true);
    }

    {
        auto& s = *reinterpret_cast<xhci_trb_status_stage*>(&req_status.trb);

        memset(&s, 0, sizeof(s));

        s.ioc = 1;
        s.trb_type = (uint16_t)xhci_trb_type::status_stage;
        s.dir = buf.empty() || !(bmRequestType & USB_REQUEST_TYPE_DEVICE_TO_HOST);

        submit(req_status, true);
    }

    wait_for_object(req_status.event.get(), false);

    if ((enum TRB_CODE)req_status.ev.completion_code != TRB_CODE::SUCCESS) {
        print("xhci_queue::control_transfer: controller returned error {}\n", (enum TRB_CODE)req_status.ev.completion_code);
        return false;
    }

    return true;
}

bool usb_device::get_descriptor(span<uint8_t> buf, enum usb_descriptor desc_type,
                                uint8_t desc_index) {
    if (!ep0_queue.control_transfer(buf,
            USB_REQUEST_TYPE_DEVICE_TO_HOST | USB_REQUEST_TYPE_STANDARD | USB_REQUEST_TYPE_RECIPIENT_DEVICE,
            usb_request::GET_DESCRIPTOR, (uint16_t)(((uint8_t)desc_type << 8) | desc_index), 0)) {
        print("xhci_queue::control_transfer failed\n");
        return false;
    }

    return true;
}

bool usb_device::get_descriptor_class(span<uint8_t> buf, enum usb_descriptor desc_type,
                                      uint8_t desc_index) {
    if (!ep0_queue.control_transfer(buf,
            USB_REQUEST_TYPE_DEVICE_TO_HOST | USB_REQUEST_TYPE_CLASS | USB_REQUEST_TYPE_RECIPIENT_DEVICE,
            usb_request::GET_DESCRIPTOR, (uint16_t)(((uint8_t)desc_type << 8) | desc_index), 0)) {
        print("xhci_queue::control_transfer failed\n");
        return false;
    }

    return true;
}

bool usb_device::set_configuration(uint8_t configuration_value) {
    if (!ep0_queue.control_transfer(span<uint8_t>(),
            USB_REQUEST_TYPE_HOST_TO_DEVICE | USB_REQUEST_TYPE_STANDARD | USB_REQUEST_TYPE_RECIPIENT_DEVICE,
            usb_request::SET_CONFIGURATION, configuration_value, 0)) {
        print("xhci_queue::control_transfer failed\n");
        return false;
    }

    return true;
}

bool usb_device::set_feature(uint8_t bmRequestType, uint8_t feature, uint16_t index) {
    if (!ep0_queue.control_transfer(span<uint8_t>(), bmRequestType,
                                    usb_request::SET_FEATURE, feature, index)) {
        print("xhci_queue::control_transfer failed\n");
        return false;
    }

    return true;
}

bool usb_device::clear_feature(uint8_t bmRequestType, uint8_t feature, uint16_t index) {
    if (!ep0_queue.control_transfer(span<uint8_t>(), bmRequestType,
                                    usb_request::CLEAR_FEATURE, feature, index)) {
        print("xhci_queue::control_transfer failed\n");
        return false;
    }

    return true;
}

bool usb_device::get_status(uint8_t bmRequestType, span<uint8_t> buf,
                            uint16_t wValue, uint16_t wIndex) {
    if (!ep0_queue.control_transfer(buf, bmRequestType, usb_request::GET_STATUS,
                                    wValue, wIndex)) {
        print("xhci_queue::control_transfer failed\n");
        return false;
    }

    return true;
}

bool xhci_queue::normal_transfer(span<uint8_t> buf) {
    xhci_request req;

    // FIXME - can set IDT flag on short OUT transfers

    {
        auto& s = *reinterpret_cast<xhci_trb_normal*>(&req.trb);

        memset(&s, 0, sizeof(s));

        s.data_buffer = va_to_pa_phys(buf.data());
        s.trb_transfer_length = (uint16_t)buf.size();

        s.ioc = 1;
        s.trb_type = (uint16_t)xhci_trb_type::normal;

        submit(req, true);
    }

    wait_for_object(req.event.get(), false);

    if ((enum TRB_CODE)req.ev.completion_code != TRB_CODE::SUCCESS) {
        print("xhci_queue::normal_transfer: controller returned error {}\n", (enum TRB_CODE)req.ev.completion_code);
        return false;
    }

    return true;
}

uint8_t xhci::get_port_slot_type(uint8_t port_id) {
    volatile auto& cap_regs = *(xhci_cap_regs*)dev.bar[0].data();
    auto xecp = ((cap_regs.HCCPARAMS1) & XHCI_HCCPARAMS1_XECP) >> XHCI_HCCPARAMS1_XECP_SHIFT;

    // FIXME - make sure we don't overflow BAR

    volatile auto* ptr = (uint32_t*)(dev.bar[0].data() + (xecp << 2));

    while (true) {
        xhci_extended_capability cap;

        *(uint32_t*)&cap = *ptr;

        if (cap.cap_type == xhci_capability_type::supported_protocol) {
            xhci_supported_protocol prot;

            // Qemu insists that we read 32 bits at a time
            copy_by_uint32(ptr, &prot, sizeof(prot));

            if (port_id >= prot.port_offset && port_id < prot.port_offset + prot.port_count)
                return prot.slot_type;
        }

        if (cap.next_cap == 0)
            break;

        ptr += cap.next_cap;
    }

    print("xhci::get_port_slot_type: could not find port {} in capabilities\n", port_id);

    return 0;
}

export void print_usb_config(span<const uint8_t> buf) {
    while (!buf.empty()) {
        auto len = buf[0];

        if (len > buf.size() || len < 2) {
            print("buffer truncated\n");
            return;
        }

        auto type = (enum usb_descriptor)buf[1];

        switch (type) {
            case usb_descriptor::DEVICE: {
                auto& d = *(usb_device_descriptor*)buf.data();

                if (len < sizeof(usb_device_descriptor)) {
                    print("buffer truncated\n");
                    return;
                }

                print("DEVICE: bcdUSB = {:x}, bDeviceClass = {}, bDeviceSubClass = {}, bDeviceProtocol = {}, bMaxPacketSize0 = {}, idVendor = {:04x}, idProduct = {:04x}, bcdDevice = {:x}, iManufacturer = {}, iProduct = {}, iSerialNumber = {}, bNumConfigurations = {}\n",
                      d.bcdUSB, d.bDeviceClass, d.bDeviceSubClass, d.bDeviceProtocol, d.bMaxPacketSize0,
                      d.idVendor, d.idProduct, d.bcdDevice, d.iManufacturer, d.iProduct, d.iSerialNumber,
                      d.bNumConfigurations);
                break;
            }

            case usb_descriptor::CONFIGURATION: {
                auto& c = *(usb_configuration_descriptor*)buf.data();

                if (len < sizeof(usb_configuration_descriptor)) {
                    print("buffer truncated\n");
                    return;
                }

                print("CONFIGURATION: wTotalLength = {}, bNumInterfaces = {}, bConfigurationValue = {}, iConfiguration = {}, bmAttributes = {:x}, bMaxPower = {}\n",
                      c.wTotalLength, c.bNumInterfaces, c.bConfigurationValue, c.iConfiguration, c.bmAttributes, c.bMaxPower);
                break;
            }

            // FIXME - STRING

            case usb_descriptor::INTERFACE: {
                auto& i = *(usb_interface_descriptor*)buf.data();

                if (len < sizeof(usb_interface_descriptor)) {
                    print("buffer truncated\n");
                    return;
                }

                print("INTERFACE: bInterfaceNumber = {}, bAlternateSetting = {}, bNumEndpoints = {}, bInterfaceClass = {}, bInterfaceSubClass = {}, bInterfaceProtocol = {}, iInterface = {}\n",
                      i.bInterfaceNumber, i.bAlternateSetting, i.bNumEndpoints, i.bInterfaceClass, i.bInterfaceSubClass, i.bInterfaceProtocol, i.iInterface);
                break;
            }

            case usb_descriptor::ENDPOINT: {
                auto& e = *(usb_endpoint_descriptor*)buf.data();

                if (len < sizeof(usb_endpoint_descriptor)) {
                    print("buffer truncated\n");
                    return;
                }

                print("ENDPOINT: endpoint = {} {}, transfer_type = {:x}, synchronization_type = {:x}, usage_type = {:x}, wMaxPacketSize = {}, bInterval = {}\n",
                      e.endpoint_number, e.direction == USB_ENDPOINT_DIRECTION_IN ? "IN" : "OUT",
                      e.transfer_type, e.synchronization_type, e.usage_type,
                      e.wMaxPacketSize, e.bInterval);
                break;
            }

            // FIXME - INTERFACE_POWER
            // FIXME - OTG
            // FIXME - DEBUG

            case usb_descriptor::INTERFACE_ASSOCIATION: {
                auto& i = *(usb_interface_association_descriptor*)buf.data();

                if (len < sizeof(usb_interface_association_descriptor)) {
                    print("buffer truncated\n");
                    return;
                }

                print("INTERFACE ASSOCIATION: bFirstInterface = {}, bInterfaceCount = {}, bFunctionClass = {}, bFunctionSubClass = {}, bFunctionProtocol = {}, iFunction = {}\n",
                      i.bFirstInterface, i.bInterfaceCount, i.bFunctionClass, i.bFunctionSubClass, i.bFunctionProtocol, i.iFunction);
                break;
            }

            // FIXME - BOS
            // FIXME - DEVICE_CAPABILITY

            case usb_descriptor::HID: {
                auto& h = *(usb_hid_descriptor*)buf.data();

                if (len < sizeof(usb_hid_descriptor)) {
                    print("buffer truncated\n");
                    return;
                }

                print("HID: bcdHID = {:x}, bCountryCode = {}, bNumDescriptors = {}, bDescriptorType2 = {}, wDescriptorLength = {}\n",
                      h.bcdHID, h.bCountryCode, h.bNumDescriptors, h.bDescriptorType2, h.wDescriptorLength);

                break;
            }

            case usb_descriptor::HUB: {
                auto& h = *(usb_hub_descriptor*)buf.data();

                if (len < offsetof(usb_hub_descriptor, DeviceRemovable)) {
                    print("buffer truncated\n");
                    return;
                }

                unsigned int dev_rem_bytes = (h.bNbrPorts / 8) + 1;

                if (len < offsetof(usb_hub_descriptor, DeviceRemovable) + dev_rem_bytes) {
                    print("buffer truncated\n");
                    return;
                }

                print("HUB: bNbrPorts = {}, wHubCharacteristics = {:04x}, bPwrOn2PwrGood = {}, bHubContrCurrent = {}, DeviceRemovable = ",
                      h.bNbrPorts, h.wHubCharacteristics, h.bPwrOn2PwrGood, h.bHubContrCurrent);

                for (unsigned int i = 0; i < dev_rem_bytes; i++) {
                    print("{:02x}", h.DeviceRemovable[i]);
                }

                print("\n");

                break;
            }

            // FIXME - SUPERSPEED_USB_ENDPOINT_COMPANION
            // FIXME - SUPERSPEEDPLUS_ISOCHRONOUS_ENDPOINT_COMPANION

            default:
                print("unrecognized USB descriptor type {}\n", (uint8_t)type);
                break;
        }

        buf = buf.subspan(len);
    }
}

static void probe_interfaces(usb_device& d) {
    auto buf = span((uint8_t*)d.config, d.config_size);

    while (!buf.empty()) {
        auto len = buf[0];

        if (len > buf.size() || len < 2) {
            print("buffer truncated\n");
            return;
        }

        auto type = (enum usb_descriptor)buf[1];

        switch (type) {
            case usb_descriptor::INTERFACE: {
                auto& i = *(usb_interface_descriptor*)buf.data();

                if (len < sizeof(usb_interface_descriptor)) {
                    print("buffer truncated\n");
                    return;
                }

                for (auto le = usb_interfaces.next; le != &usb_interfaces; le = le->next) {
                    auto& r = containing_record<usb_recognizer>(le);

                    if (r.interface_code == i.bInterfaceClass && r.func(d, i.bInterfaceNumber, i.bAlternateSetting))
                        break;
                }

                break;
            }

            default:
                break;
        }

        buf = buf.subspan(len);
    }
}

pair<uint32_t, uint16_t> xhci::get_speed(uint8_t port_id, uint8_t speed) {
    volatile auto& cap_regs = *(xhci_cap_regs*)dev.bar[0].data();
    auto xecp = ((cap_regs.HCCPARAMS1) & XHCI_HCCPARAMS1_XECP) >> XHCI_HCCPARAMS1_XECP_SHIFT;

    // FIXME - make sure we don't overflow BAR

    static const uint32_t default_speeds[] = {
        0x000c0021, // full-speed
        0x05dc0012, // low-speed
        0x01e00023, // high-speed
        0x00050134, // superspeed gen1 x1
        0x000a4135, // superspeed+ gen2 x1
        0x000a4136, // superspeed+ gen1 x2
        0x00144137  // superspeed+ gen2 x2
    };

    volatile auto* ptr = (uint32_t*)(dev.bar[0].data() + (xecp << 2));

    while (true) {
        xhci_extended_capability cap;

        *(uint32_t*)&cap = *ptr;

        if (cap.cap_type == xhci_capability_type::supported_protocol) {
            xhci_supported_protocol prot;

            // Qemu insists that we read 32 bits at a time
            copy_by_uint32(ptr, &prot, sizeof(prot));

            // FIXME - check name string

            auto ptr2 = ptr + (sizeof(prot) / sizeof(uint32_t));
            if (port_id >= prot.port_offset && port_id < prot.port_offset + prot.port_count) {
                optional<uint32_t> found_speed = 0;

                for (unsigned int i = 0; i < prot.psic; i++) {
                    xhci_psi psi;

                    copy_by_uint32(ptr2, &psi, sizeof(psi));
                    ptr2++;

                    if (psi.psiv == speed) {
                        found_speed = *(uint32_t*)&psi;
                        break;
                    }
                }

                if (prot.psic == 0) {
                    if (prot.revision_major == 2 && prot.revision_minor == 0) { // USB 2.0
                        if (speed >= 1 && speed <= 3)
                            found_speed = default_speeds[speed - 1];
                    } else if (prot.revision_major == 3 && prot.revision_minor == 0) { // USB 3.0
                        if (speed == 4)
                            found_speed = default_speeds[speed - 1];
                    } else if (prot.revision_major == 3 && prot.revision_minor == 0x10) { // USB 3.1
                        if (speed >= 4 && speed <= 5)
                            found_speed = default_speeds[speed - 1];
                    } else if (prot.revision_major == 3 && prot.revision_minor == 0x20) { // USB 3.2
                        if (speed >= 4 && speed <= 7)
                            found_speed = default_speeds[speed - 1];
                    }
                }

                if (!found_speed.has_value()) {
                    print("xhci::get_speed: could not translate PSIV {} into speed for port {}\n", speed, port_id);
                    return { 0, 0 };
                }

                return { *found_speed, (prot.revision_major << 8) | prot.revision_minor };
            }
        }

        if (cap.next_cap == 0)
            break;

        ptr += cap.next_cap;
    }

    print("xhci::get_speed: could not find port {} in capabilities\n", port_id);

    return { 0, 0 };
}

uint16_t xhci::get_port_usb_revision(uint8_t port_id) {
    volatile auto& cap_regs = *(xhci_cap_regs*)dev.bar[0].data();
    auto xecp = ((cap_regs.HCCPARAMS1) & XHCI_HCCPARAMS1_XECP) >> XHCI_HCCPARAMS1_XECP_SHIFT;

    // FIXME - make sure we don't overflow BAR

    volatile auto* ptr = (uint32_t*)(dev.bar[0].data() + (xecp << 2));

    while (true) {
        xhci_extended_capability cap;

        *(uint32_t*)&cap = *ptr;

        if (cap.cap_type == xhci_capability_type::supported_protocol) {
            xhci_supported_protocol prot;

            // Qemu insists that we read 32 bits at a time
            copy_by_uint32(ptr, &prot, sizeof(prot));

            // FIXME - check name string

            if (port_id >= prot.port_offset && port_id < prot.port_offset + prot.port_count)
                return (uint16_t)(prot.revision_major << 8) | prot.revision_minor;
        }

        if (cap.next_cap == 0)
            break;

        ptr += cap.next_cap;
    }

    print("xhci::get_port_usb_revision: could not find port {} in capabilities\n", port_id);

    return 0;
}

void xhci::device_found(uint8_t slot, uint8_t root_port, uint32_t route_string,
                        uint16_t usb_revision, uint32_t psi_num, uint8_t port_speed,
                        unsigned int depth) {
    usb_device* d;

    // wait 10ms for device to come up

    {
        auto tim_ref = create_timer(false);
        if (!tim_ref) {
            print("xhci::device_found: create_timer failed\n");
            return;
        }

        auto& tim = static_cast<timer&>(tim_ref.get());

        auto status = tim.set(-ONE_MS_TICKS * 10, nullptr, nullptr, false,
                              0, nullptr);
        if (!success(status)) {
            print("xhci::device_found: timer::set returned {}\n", status);
            return;
        }

        wait_for_object(tim, true);
    }

    auto func = [&]<bool LARGE>() -> bool {
        auto devctx = new xhci_device_context<LARGE>;
        if (!devctx) {
            print("new failed\n");
            // FIXME - disable slot
            return false;
        }

        memset(devctx, 0, sizeof(*devctx));

        dcbaa[slot] = va_to_pa_phys(devctx);

        d = new usb_device(*this, devctx, slot, port_speed, root_port,
                           route_string, usb_revision, *(xhci_psi*)&psi_num,
                           depth);
        if (!d) {
            print("new failed\n");
            delete devctx;
            dcbaa[slot] = 0;
            // FIXME - disable slot
            return false;
        }

        if (!d->initialized) {
            print("usb_device::usb_device failed\n");
            delete d;
            return false;
        }

        print("received address {}\n", devctx->slot.usb_device_address);

        return true;
    };

    if (large_contexts) {
        if (!func.template operator()<true>())
            return;
    } else {
        if (!func.template operator()<false>())
            return;
    }

    if (!d->get_descriptor(span((uint8_t*)&d->device_descriptor, sizeof(d->device_descriptor)),
                          usb_descriptor::DEVICE, 0)) {
        print("usb_device::get_descriptor failed\n");
        d->release();
        return;
    }

    print("found USB device {:04x}:{:04x}\n", d->device_descriptor.idVendor,
          d->device_descriptor.idProduct);

    print_usb_config(span((uint8_t*)&d->device_descriptor, sizeof(d->device_descriptor)));

    auto cd = new usb_configuration_descriptor;
    if (!cd) {
        print("new failed\n");
        d->release();
        return;
    }

    // FIXME - support multiple configurations?

    if (!d->get_descriptor(span((uint8_t*)cd, sizeof(*cd)),
                           usb_descriptor::CONFIGURATION, 0)) {
        print("usb_device::get_descriptor failed\n");
        delete cd;
        d->release();
        return;
    }

    if (cd->wTotalLength < sizeof(*cd)) {
        print("xhci::port_status_changed: config descriptor was {} bytes, expected at least {}\n",
              cd->wTotalLength, sizeof(*cd));
        delete cd;
        d->release();
        return;
    }

    d->config_size = cd->wTotalLength;

    delete cd;

    if (d->config_size < PAGE_SIZE) {
        d->config = malloc(d->config_size);
        if (!d->config) {
            print("malloc failed\n");
            d->release();
            return;
        }
    } else {
        auto f = allocate_folio(bytes_to_folio_order(d->config_size));
        if (!f) {
            print("allocate_folio failed\n");
            d->release();
            return;
        }

        d->config = phys_address(f);
    }

    if (!d->get_descriptor(span((uint8_t*)d->config, d->config_size),
                          usb_descriptor::CONFIGURATION, 0)) {
        print("usb_device::get_descriptor failed\n");
        d->release();
        return;
    }

    print_usb_config(span((uint8_t*)d->config, d->config_size));

    auto& config = *(usb_configuration_descriptor*)d->config;

    if (!d->set_configuration(config.bConfigurationValue)) {
        print("usb_device::set_configuration failed\n");
        d->release();
        return;
    }

    probe_interfaces(*d);
}

void xhci::port_status_changed(uint8_t port_id) {
    print("xhci::port_status_changed: {}\n", port_id);

    if (port_id == 0 || port_id > num_ports) {
        print("xhci::port_status_changed: port_id {} out of range (expected 1 to {})\n", port_id, num_ports);
        return;
    }

    volatile auto& op_regs = *(xhci_op_regs*)((uint8_t*)dev.bar[0].data() + caplength);
    xhci_portsc portsc;

    copy_by_uint32(&op_regs.ports[port_id - 1].PORTSC, &portsc, sizeof(portsc));

    bool device_present = portsc.CCS;

    auto& p = ports[port_id - 1];

    // ignore spurious messages from controller
    if ((device_present && p.slot != 0) || (!device_present && p.slot == 0))
        return;

    if (!device_present) {
        // FIXME - disable slot
        // FIXME - if hub, handle child devices
        return;
    }

    // USB2 ports have to be reset before their speeds are valid
    if (!portsc.PRC) {
        if (!reset_port(port_id))
            return;

        copy_by_uint32(&op_regs.ports[port_id - 1].PORTSC, &portsc,
                       sizeof(portsc));
    }

    auto slot = enable_slot(get_port_slot_type(port_id));
    if (slot == 0) {
        print("xhci::port_status_changed: could not enable slot\n");
        return;
    }

    auto [psi_num, usb_revision] = get_speed(port_id, portsc.port_speed);
    if (psi_num == 0) {
        print("xhci::get_speed failed\n");
        // FIXME - disable slot
        return;
    }

    p.slot = slot;

    device_found(slot, port_id, 0, usb_revision, psi_num, portsc.port_speed, 0);
}

void xhci::port_status_change_thread() {
    while (true) {
        wait_for_object(port_status_change_event.get(), true);

        while (true) {
            port_status_change* ent = nullptr;

            {
                lock_guard lock(port_status_change_lock);

                if (!is_list_empty(port_status_change_list)) {
                    ent = &containing_record<port_status_change>(port_status_change_list.next);

                    remove_entry_list(ent->le);
                }
            }

            if (!ent)
                break;

            auto port_id = ent->port_id;
            delete ent;

            port_status_changed(port_id);
        }
    }
}

void xhci::isr_port_status_change(xhci_trb_port_status_change& psc) {
    // FIXME - doing allocation inside ISR isn't brilliant
    auto ent = new port_status_change;
    if (!ent) {
        print("new failed\n");
        return;
    }

    ent->port_id = psc.port_id;

    {
        lock_guard lock(port_status_change_lock);
        insert_tail_list(port_status_change_list, ent->le);
    }

    set_event(static_cast<event&>(port_status_change_event.get()));
}

void xhci_queue::isr_transfer_event(const xhci_trb_transfer_event& ev) {
    if (ev.trb_pointer < ring_pa || ev.trb_pointer >= ring_pa + (size * sizeof(xhci_trb_common)))
        return;

    size_t index = (size_t)(ev.trb_pointer - ring_pa) / sizeof(xhci_trb_common);
    auto& req = *reqs[index];

    memcpy(&req.ev, &ev, sizeof(ev));

    set_event(static_cast<event&>(req.event.get()));
}

void usb_device::isr_transfer_event(const xhci_trb_transfer_event& ev) {
    if (ev.endpoint_id == 1) {
        ep0_queue.isr_transfer_event(ev);
        return;
    }

    uint8_t num = ev.endpoint_id >> 1;
    bool dir = ev.endpoint_id & 1;

    {
        lock_guard lock(endpoint_spinlock);

        for (auto le = endpoints.next; le != &endpoints; le = le->next) {
            auto& ep = containing_record<device_endpoint>(le);

            if (ep.num == num && !!ep.dir == !!dir) {
                ep.queue.isr_transfer_event(ev);
                return;
            }
        }
    }
}

void xhci::isr() {
    volatile auto& run_regs = *(xhci_run_regs*)(dev.bar[0].data() + rtsoff);
    volatile auto& int_reg = run_regs.int_regs[0];

    while (true) {
        auto& ev = event_ring[event_ring_offset];

        if (!!ev.cycle != !!event_ring_cycle)
            break;

        switch ((enum xhci_trb_type)ev.trb_type) {
            case xhci_trb_type::command_completion_event:
                isr_command_completion(*reinterpret_cast<xhci_trb_command_completion*>(&ev));
                break;

            case xhci_trb_type::port_status_change_event:
                isr_port_status_change(*reinterpret_cast<xhci_trb_port_status_change*>(&ev));
                break;

            case xhci_trb_type::transfer_event: {
                auto& ev2 = *reinterpret_cast<xhci_trb_transfer_event*>(&ev);

                usb_device* found_device = nullptr;

                {
                    lock_guard lock(devices_lock);

                    for (auto le = devices.next; le != &devices; le = le->next) {
                        auto& d = containing_record<usb_device>(le);

                        if (d.slot == ev2.slot_id) {
                            found_device = &d;
                            d.acquire();
                            break;
                        }
                    }
                }

                if (found_device) {
                    found_device->isr_transfer_event(ev2);
                    found_device->release();
                }

                break;
            }

            default:
                print("xhci::isr: event type {}\n", ev.trb_type); // FIXME?
                break;
        }

        event_ring_offset++;

        if (event_ring_offset == EVENT_RING_SIZE) {
            event_ring_offset = 0;
            event_ring_cycle = !event_ring_cycle;
        }
    }

    write_volatile64(&int_reg.ERDP, (event_ring_pa + (event_ring_offset * sizeof(xhci_trb_common))) | XHCI_ERDP_EHB);
}

void xhci_queue::submit(xhci_request& req, bool ring_bell) {
    req.event = create_event(true);

    {
        lock_guard lock(submit_lock);

        auto& trb = *(xhci_trb_common*)((uint8_t*)ring + (index * sizeof(xhci_trb_common)));

        req.trb.cycle = cycle_bit ? 1 : 0;
        memcpy(&trb, &req.trb, sizeof(trb));

        reqs[index] = &req;

        if (index == (unsigned int)(size - 1)) {
            auto& cmd_link = *reinterpret_cast<xhci_trb_link*>(&ring[size]);
            cmd_link.cycle = cycle_bit ? 1 : 0;

            cycle_bit = !cycle_bit;
        }

        barrier();

        if (ring_bell)
            *doorbell = doorbell_val;

        index++;

        if (index == size)
            index = 0;
    }
}

uint8_t xhci::enable_slot(uint8_t slot_type) {
    xhci_request req;

    auto& s = *reinterpret_cast<xhci_trb_enable_slot*>(&req.trb);

    memset(&s, 0, sizeof(s));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    s.slot_type = slot_type;
#pragma GCC diagnostic pop
    s.trb_type = (uint32_t)xhci_trb_type::enable_slot_command;

    command_queue.submit(req, true);

    wait_for_object(req.event.get(), false);

    // FIXME - translate into nt_status?
    if ((enum TRB_CODE)req.comp.completion_code != TRB_CODE::SUCCESS) {
        print("xhci::enable_slot: controller returned error {}\n", (enum TRB_CODE)req.comp.completion_code);
        return 0;
    }

    return req.comp.slot_id;
}

export
extern "C++"
void init_xhci(pci_device& dev) {
    auto x = new xhci(dev);
    if (!x) {
        print("new failed\n");
        return;
    }

    if (!x->initialized) {
        delete x;
        return;
    }

    x->init_ports();

    // FIXME - add to list of xhci controllers?
}
