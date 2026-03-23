module;

#include <atomic>
#include <stdint.h>
#include <stdlib.h>
#include <string_view>
#include <span>
#include "xhci.h"

export module xhci;

import pci;
import usb;
import rwsem;
import object_ref;
import list;
import spinlock;
import page_cache;

using namespace std;

export class usb_device;

struct xhci_request;

export class xhci_queue {
public:
    xhci_queue(usb_device* dev) : dev(dev) { }

    ~xhci_queue();
    void initialize(uint16_t size, volatile uint32_t* doorbell,
                    uint32_t doorbell_val);
    void submit(xhci_request& req, bool ring_bell);
    bool control_transfer(span<uint8_t> buf, uint8_t bmRequestType,
                          enum usb_request bRequest, uint16_t wValue,
                          uint16_t wIndex);
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
    void submit_holding_lock(xhci_request& req, bool ring_bell);

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
