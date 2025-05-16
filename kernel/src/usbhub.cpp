module;

#include "kernel.h"
#include <span>

export module usbhub;

import print;
import format;
import xhci;
import usb;
import mem;
import timer;

using namespace std;

static const unsigned int PORT_RESET_DELAY = 800; // ms

#define ONE_MS_TICKS 10000 // FIXME

union port_status {
    struct {
        uint16_t connection : 1;
        uint16_t enabled : 1;
        uint16_t suspended : 1;
        uint16_t over_current : 1;
        uint16_t reset : 1;
        uint16_t : 3;
        uint16_t power : 1;
        uint16_t low_speed : 1;
        uint16_t high_speed : 1;
        uint16_t test_mode : 1;
        uint16_t indicator_control : 1;
        uint16_t : 3;
        uint16_t connection_changed : 1;
        uint16_t enable_changed : 1;
        uint16_t suspend_changed : 1;
        uint16_t over_current_changed : 1;
        uint16_t reset_changed : 1;
        uint16_t : 11;
    } usb2;
    struct {
        uint16_t connection : 1;
        uint16_t enabled : 1;
        uint16_t : 1;
        uint16_t over_current : 1;
        uint16_t reset : 1;
        uint16_t link_state : 4;
        uint16_t power : 1;
        uint16_t speed : 3;
        uint16_t : 3;
        uint16_t connection_changed : 1;
        uint16_t : 2;
        uint16_t over_current_changed : 1;
        uint16_t reset_changed : 1;
        uint16_t bh_reset_changed : 1;
        uint16_t link_state_changed : 1;
        uint16_t config_error : 1;
        uint16_t : 8;
    } usb3;
} __attribute((packed));

static_assert(sizeof(port_status) == 0x4);

struct usb_hub_port {
    port_status status;
    uint8_t slot;
};

class usb_device_hub : public usb_hub {
public:
    usb_device_hub(usb_device& d, xhci_queue& queue, usb2_hub_descriptor* hub_desc,
                   uint8_t num_ports, bool multi_tt);
    usb_device_hub(usb_device& d, xhci_queue& queue, usb3_hub_descriptor* hub_desc,
                   uint8_t num_ports);
    ~usb_device_hub();
    void recv_thread();
    void kill_children();

    bool initialized = false;

private:
    bool set_port_feature(enum usb2_port_feature_selector feature, uint16_t index);
    bool set_port_feature(enum usb3_port_feature_selector feature, uint16_t index);
    bool clear_port_feature(enum usb2_port_feature_selector feature,
                            uint16_t index);
    bool get_port_status(uint8_t port, port_status& status);
    void port_status_change(uint8_t port);
    void device_connected(uint8_t port);
    bool get_hub_status(uint32_t& hub_status);
    void device_removed(uint8_t port);

    usb_device& d;
    xhci_queue& queue;

    union {
        usb2_hub_descriptor usb2;
        usb3_hub_descriptor usb3;
    } hub_desc;

    usb_hub_port* ports = nullptr;
    uint32_t hub_status;
};

static bool found_usb_hub_device(usb_device& d, const usb_device_config& config,
                                 const usb_interface& interface);

static usb_recognizer mod_usb_interfaces = {
    {}, usb_class_code::hub, found_usb_hub_device
};

static void init_usbhub() {
    insert_tail_list(usb_interfaces, mod_usb_interfaces.le);
}

static module_init module_init_usbhub(init_usbhub);

template<>
consteval bool format_spec_valid<port_status>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, const port_status& arg) {
    sp = do_format_arg(sp, "", "connection = ");
    sp = do_format_arg(sp, "", arg.usb2.connection);
    sp = do_format_arg(sp, "", ", enabled = ");
    sp = do_format_arg(sp, "", arg.usb2.enabled);
    sp = do_format_arg(sp, "", ", suspended = ");
    sp = do_format_arg(sp, "", arg.usb2.suspended);
    sp = do_format_arg(sp, "", ", over_current = ");
    sp = do_format_arg(sp, "", arg.usb2.over_current);
    sp = do_format_arg(sp, "", ", reset = ");
    sp = do_format_arg(sp, "", arg.usb2.reset);
    sp = do_format_arg(sp, "", ", power = ");
    sp = do_format_arg(sp, "", arg.usb2.power);
    sp = do_format_arg(sp, "", ", low_speed = ");
    sp = do_format_arg(sp, "", arg.usb2.low_speed);
    sp = do_format_arg(sp, "", ", high_speed = ");
    sp = do_format_arg(sp, "", arg.usb2.high_speed);
    sp = do_format_arg(sp, "", ", test_mode = ");
    sp = do_format_arg(sp, "", arg.usb2.test_mode);
    sp = do_format_arg(sp, "", ", indicator_control = ");
    sp = do_format_arg(sp, "", arg.usb2.indicator_control);
    sp = do_format_arg(sp, "", ", connection_changed = ");
    sp = do_format_arg(sp, "", arg.usb2.connection_changed);
    sp = do_format_arg(sp, "", ", enable_changed = ");
    sp = do_format_arg(sp, "", arg.usb2.enable_changed);
    sp = do_format_arg(sp, "", ", suspend_changed = ");
    sp = do_format_arg(sp, "", arg.usb2.suspend_changed);
    sp = do_format_arg(sp, "", ", over_current_changed = ");
    sp = do_format_arg(sp, "", arg.usb2.over_current_changed);
    sp = do_format_arg(sp, "", ", reset_changed = ");
    sp = do_format_arg(sp, "", arg.usb2.reset_changed);

    return sp;
}

bool usb_device_hub::get_hub_status(uint32_t& hub_status) {
    auto buf = (uint32_t*)malloc(sizeof(uint32_t));
    if (!buf) {
        print("malloc failed\n");
        return false;
    }

    if (!d.get_status(USB_REQUEST_TYPE_DEVICE_TO_HOST | USB_REQUEST_TYPE_CLASS | USB_REQUEST_TYPE_RECIPIENT_DEVICE,
                      span((uint8_t*)buf, sizeof(*buf)), 0, 0)) {
        print("usb_device::get_status failed\n");
        free(buf);
        return false;
    }

    hub_status = *buf;
    free(buf);

    return true;
}

usb_device_hub::usb_device_hub(usb_device& d, xhci_queue& queue, usb2_hub_descriptor* hub_desc,
                               uint8_t num_ports, bool multi_tt) :
        usb_hub(d.controller, &d, d.parent.depth + 1), d(d), queue(queue) {
    this->multi_tt = multi_tt;
    this->num_ports = num_ports;

    memcpy(&this->hub_desc.usb2, hub_desc,
           min(sizeof(usb2_hub_descriptor), (size_t)hub_desc->bDescLength));

    d.acquire();

    ports = (usb_hub_port*)malloc(sizeof(usb_hub_port) * num_ports);
    if (!ports) {
        print("malloc failed\n");
        return;
    }

    memset(ports, 0, sizeof(usb_hub_port) * num_ports);

    children = (usb_device**)malloc(sizeof(usb_device*) * num_ports);
    if (!children) {
        print("malloc failed\n");
        return;
    }

    memset(children, 0, sizeof(usb_device*) * num_ports);

    if (!get_hub_status(hub_status)) {
        print("usb_device_hub::get_hub_status failed\n");
        return;
    }

    for (uint8_t port = 1; port <= num_ports; port++) {
        if (!set_port_feature(usb2_port_feature_selector::PORT_POWER, port)) {
            print("usb_device_hub::set_port_feature failed\n");
            return;
        }
    }

    for (uint8_t port = 1; port <= num_ports; port++) {
        if (!set_port_feature(usb2_port_feature_selector::PORT_RESET, port)) {
            print("usb_device_hub::set_port_feature(PORT_RESET) failed\n");
            return;
        }
    }

    initialized = true;
}

usb_device_hub::usb_device_hub(usb_device& d, xhci_queue& queue, usb3_hub_descriptor* hub_desc,
                               uint8_t num_ports) :
        usb_hub(d.controller, &d, d.parent.depth + 1), d(d), queue(queue) {
    this->multi_tt = multi_tt;
    this->num_ports = num_ports;

    memcpy(&this->hub_desc.usb3, hub_desc,
           min(sizeof(usb3_hub_descriptor), (size_t)hub_desc->bDescLength));

    d.acquire();

    ports = (usb_hub_port*)malloc(sizeof(usb_hub_port) * num_ports);
    if (!ports) {
        print("malloc failed\n");
        return;
    }

    memset(ports, 0, sizeof(usb_hub_port) * num_ports);

    children = (usb_device**)malloc(sizeof(usb_device*) * num_ports);
    if (!children) {
        print("malloc failed\n");
        return;
    }

    memset(children, 0, sizeof(usb_device*) * num_ports);

    if (!get_hub_status(hub_status)) {
        print("usb_device_hub::get_hub_status failed\n");
        return;
    }

    for (uint8_t port = 1; port <= num_ports; port++) {
        if (!set_port_feature(usb3_port_feature_selector::PORT_POWER, port)) {
            print("usb_device_hub::set_port_feature failed\n");
            return;
        }
    }

    initialized = true;
}

usb_device_hub::~usb_device_hub() {
    d.release();

    if (ports)
        free(ports);
}

void usb_device_hub::device_connected(uint8_t port) {
    auto& controller = d.controller;
    auto& p = ports[port - 1];

    if (!set_port_feature(usb2_port_feature_selector::PORT_RESET, port)) {
        print("usb_device_hub::set_port_feature(PORT_RESET) failed\n");
        return;
    }

    {
        auto tim_ref = create_timer(true);
        if (!tim_ref) {
            print("usb_device_hub::device_connected: create_timer failed\n");
            return;
        }

        auto& tim = static_cast<timer&>(tim_ref.get());

        auto status = tim.set(-ONE_MS_TICKS * 10, nullptr, nullptr, false,
                              10, nullptr);
        if (!success(status)) {
            print("usb_device_hub::device_connected: timer::set returned {}\n", status);
            return;
        }

        for (unsigned int i = 0; i < PORT_RESET_DELAY; i += 10) {
            if (!get_port_status(port, p.status)) {
                print("usb_device_hub::get_port_status failed\n");
                return;
            }

            if (!p.status.usb2.reset)
                break;

            wait_for_object(tim, true);
        }

        if (p.status.usb2.reset) {
            print("usb_device_hub::device_connected: reset timed out\n");
            return;
        }
    }

    // FIXME - can we rely on slot_type always being 0?
    auto slot = controller.enable_slot(0);
    if (slot == 0) {
        print("usb_device_hub::device_connected: could not enable slot\n");
        return;
    }

    p.slot = slot;

    // FIXME - USB3 hubs

    enum usb_speed_class speed_class;
    uint32_t psi_num;

    if (p.status.usb2.low_speed) {
        speed_class = usb_speed_class::low_speed;
        psi_num = 0x05dc0010;
    } else if (p.status.usb2.high_speed) {
        speed_class = usb_speed_class::high_speed;
        psi_num = 0x01e00020;
    } else {
        speed_class = usb_speed_class::full_speed;
        psi_num = 0x000c0020;
    }

    auto speed = controller.get_speed_num(d.find_root_port(), psi_num);

    if (speed == 0) {
        print("usb_device_hub::device_connected: could not find speed number for port\n");
        return;
    }

    device_found(slot, port, speed_class, speed);
}

bool usb_device_hub::set_port_feature(enum usb2_port_feature_selector feature,
                                      uint16_t index) {
    if (!d.set_feature(USB_REQUEST_TYPE_HOST_TO_DEVICE | USB_REQUEST_TYPE_CLASS | USB_REQUEST_TYPE_RECIPIENT_OTHER,
                       (uint8_t)feature, index)) {
        print("usb_device::set_feature failed\n");
        return false;
    }

    return true;
}

bool usb_device_hub::set_port_feature(enum usb3_port_feature_selector feature,
                                      uint16_t index) {
    if (!d.set_feature(USB_REQUEST_TYPE_HOST_TO_DEVICE | USB_REQUEST_TYPE_CLASS | USB_REQUEST_TYPE_RECIPIENT_OTHER,
                       (uint8_t)feature, index)) {
        print("usb_device::set_feature failed\n");
        return false;
    }

    return true;
}

bool usb_device_hub::get_port_status(uint8_t port, port_status& status) {
    if (!d.get_status(USB_REQUEST_TYPE_DEVICE_TO_HOST | USB_REQUEST_TYPE_CLASS | USB_REQUEST_TYPE_RECIPIENT_OTHER,
                      span((uint8_t*)&status, sizeof(status)), 0, port)) {
        print("usb_device::get_status failed\n");
        return false;
    }

    return true;
}

bool usb_device_hub::clear_port_feature(enum usb2_port_feature_selector feature,
                                        uint16_t index) {
    if (!d.clear_feature(USB_REQUEST_TYPE_HOST_TO_DEVICE | USB_REQUEST_TYPE_CLASS | USB_REQUEST_TYPE_RECIPIENT_OTHER,
                         (uint8_t)feature, index)) {
        print("usb_device::set_feature failed\n");
        return false;
    }

    return true;
}

void usb_device_hub::device_removed(uint8_t port) {
    if (!children[port - 1]) {
        print("usb_device_hub::device_removed: no device currently in port {}\n", port);
        return;
    }

    children[port - 1]->device_removed();
}

void usb_device_hub::port_status_change(uint8_t port) {
    auto new_status = (port_status*)malloc(sizeof(port_status));
    if (!new_status) {
        print("malloc failed\n");
        return;
    }

    if (!get_port_status(port, *new_status)) {
        print("usb_device_hub::get_port_status failed\n");
        free(new_status);
        return;
    }

    if (port <= num_ports) {
        auto old_status = ports[port - 1].status;

        ports[port - 1].status = *new_status;

        if (new_status->usb2.connection_changed) {
            if (old_status.usb2.connection != new_status->usb2.connection) {
                if (new_status->usb2.connection) {
                    acquire();

                    add_kernel_thread([](void* param1, void* param2) {
                        auto hub = (usb_device_hub*)param1;
                        auto port = (uint8_t)(uintptr_t)param2;

                        hub->device_connected(port);
                        hub->release();
                    }, this, (void*)(uintptr_t)port);
                } else
                    device_removed(port);
            }

            clear_port_feature(usb2_port_feature_selector::C_PORT_CONNECTION, port);
        }
    } else {
        if (new_status->usb2.connection_changed)
            clear_port_feature(usb2_port_feature_selector::C_PORT_CONNECTION, port);
    }

    if (new_status->usb2.enable_changed)
        clear_port_feature(usb2_port_feature_selector::C_PORT_ENABLE, port);

    if (new_status->usb2.suspend_changed)
        clear_port_feature(usb2_port_feature_selector::C_PORT_SUSPEND, port);

    if (new_status->usb2.over_current_changed)
        clear_port_feature(usb2_port_feature_selector::C_PORT_OVER_CURRENT, port);

    if (new_status->usb2.reset_changed)
        clear_port_feature(usb2_port_feature_selector::C_PORT_RESET, port);

    free(new_status);
}

void usb_device_hub::recv_thread() {
    size_t msg_size = (hub_desc.usb2.bNbrPorts / 8) + 1;

    auto buf = (uint8_t*)malloc(msg_size);
    if (!buf) {
        print("malloc failed\n");
        return;
    }

    while (true) {
        if (!queue.normal_transfer(span(buf, msg_size))) {
            if (!d.dead)
                print("xhci_queue::normal_transfer failed\n");
            break;
        }

        if (buf[0] & 1) {
            print("usb_device_hub::recv_thread: hub change detected\n");
            // FIXME - process and clear change bit (C_HUB_LOCAL_POWER or C_HUB_OVER_CURRENT)
        }

        for (uint8_t i = 0; i < hub_desc.usb2.bNbrPorts; i++) {
            if (buf[(i + 1) / 8] & (1 << ((i + 1) % 8)))
                port_status_change(i + 1);
        }
    }

    free(buf);
}

static usb2_hub_descriptor* get_usb2_hub_descriptor(usb_device& d) {
    size_t size = offsetof(usb2_hub_descriptor, DeviceRemovable);

    auto desc = (usb2_hub_descriptor*)malloc(size);
    if (!desc) {
        print("malloc failed\n");
        return nullptr;
    }

    if (!d.get_descriptor_class(span((uint8_t*)desc, size), usb_descriptor::HUB, 0)) {
        print("found_usb_hub_interface: could not get HUB descriptor\n");
        free(desc);
        return nullptr;
    }

    if (desc->bDescLength > size) {
        size = desc->bDescLength;

        free(desc);

        desc = (usb2_hub_descriptor*)malloc(size);
        if (!desc) {
            print("malloc failed\n");
            return nullptr;
        }

        if (!d.get_descriptor_class(span((uint8_t*)desc, size), usb_descriptor::HUB, 0)) {
            print("found_usb_hub_interface: could not get HUB descriptor\n");
            free(desc);
            return nullptr;
        }
    }

#if 0
    print_usb_config(span((uint8_t*)desc, size));
#endif

    return desc;
}

static usb3_hub_descriptor* get_usb3_hub_descriptor(usb_device& d) {
    auto desc = new usb3_hub_descriptor;
    if (!desc) {
        print("new failed\n");
        return nullptr;
    }

    if (!d.get_descriptor_class(span((uint8_t*)desc, sizeof(usb3_hub_descriptor)),
                                usb_descriptor::SUPERSPEED_HUB, 0)) {
        print("found_usb_hub_interface: could not get SUPERSPEED_HUB descriptor\n");
        delete desc;
        return nullptr;
    }

#if 0
    print_usb_config(span((uint8_t*)desc, sizeof(usb3_hub_descriptor)));
#endif

    return desc;
}

void usb_device_hub::kill_children() {
    for (uint8_t i = 1; i < num_ports; i++) {
        if (children[i - 1])
            children[i - 1]->device_removed();
    }
}

static bool found_usb_hub_interface(usb_device& d, const usb_device_config& config,
                                    const usb_interface& interface) {
    bool multi_tt = false;
    uint8_t tt_think_time = 0;

    if (interface.desc.bAlternateSetting)
        return false;

    usb_endpoint_descriptor* ep_desc = nullptr;

    // find endpoint number

    for (auto le = interface.endpoints.next; le != &interface.endpoints; le = le->next) {
        auto& e = containing_record<usb_endpoint>(le);

        if (e.desc.direction != USB_ENDPOINT_DIRECTION_IN)
            continue;

        if (e.desc.transfer_type != USB_ENDPOINT_TRANSFER_TYPE_INTERRUPT)
            continue;

        if (ep_desc) {
            print("found_usb_hub_interface: multiple potential endpoints found\n");
            return false;
        }

        ep_desc = &e.desc;

        break;
    }

    if (!ep_desc) {
        print("found_usb_hub_interface: no potential endpoint found\n");
        return false;
    }

    if (d.speed_class == usb_speed_class::high_speed) {
        auto* interface2 = &interface;

        for (auto le = config.interfaces.next; le != &config.interfaces; le = le->next) {
            auto& i = containing_record<usb_interface>(le);

            if (i.desc.bInterfaceProtocol == 2) {
                interface2 = &i;
            }
        }

        if (interface2->desc.bAlternateSetting != 0) {
            if (!d.set_interface(interface2->desc.iInterface, interface2->desc.bAlternateSetting)) {
                print("usb_device::set_interface failed\n");
                return false;
            }
        }

        multi_tt = interface2->desc.bInterfaceProtocol == 2;
    }

    // set number of ports to 4 initially
    if (!d.configure_endpoint(*ep_desc, 4, false, 0)) {
        print("usb_device::configure_endpoint failed\n");
        return false;
    }

    usb_device_hub* hub;

    if (d.speed_class >= usb_speed_class::super_speed) {
        auto hub_desc = get_usb3_hub_descriptor(d);
        if (!hub_desc) {
            print("get_hub_descriptor failed\n");
            return false;
        }

        auto num_ports = hub_desc->bNbrPorts;

        if (num_ports == 0) {
            print("found_usb_hub_interface: number of ports was 0\n");
            free(hub_desc);
            return false;
        }

        if (num_ports > 15) {
            print("found_usb_hub_interface: bNbrPorts was {}, changing to 31\n", num_ports);
            num_ports = 15;
        }

        auto queue = d.configure_endpoint(*ep_desc, num_ports);
        if (!queue) {
            print("usb_device::configure_endpoint failed\n");
            free(hub_desc);
            return false;
        }

        hub = new usb_device_hub(d, *queue, hub_desc, num_ports);
        if (!hub) {
            print("new failed\n");
            free(hub_desc);
            return false;
        }

        free(hub_desc);
    } else {
        auto hub_desc = get_usb2_hub_descriptor(d);
        if (!hub_desc) {
            print("get_hub_descriptor failed\n");
            return false;
        }

        auto num_ports = hub_desc->bNbrPorts;

        if (num_ports == 0) {
            print("found_usb_hub_interface: number of ports was 0\n");
            free(hub_desc);
            return false;
        }

        if (num_ports > 15) {
            print("found_usb_hub_interface: bNbrPorts was {}, changing to 31\n", num_ports);
            num_ports = 15;
        }

        if (d.speed_class == usb_speed_class::high_speed)
            tt_think_time = hub_desc->tt_think_time;

        auto queue = d.configure_endpoint(*ep_desc, num_ports, multi_tt,
                                        tt_think_time);
        if (!queue) {
            print("usb_device::configure_endpoint failed\n");
            free(hub_desc);
            return false;
        }

        hub = new usb_device_hub(d, *queue, hub_desc, num_ports, multi_tt);
        if (!hub) {
            print("new failed\n");
            free(hub_desc);
            return false;
        }

        free(hub_desc);
    }

    if (!hub->initialized) {
        print("usb_device_hub::usb_device_hub failed\n");
        delete hub;
        return false;
    }

    add_kernel_thread([](void* ctx) {
        auto hub = (usb_device_hub*)ctx;

        hub->recv_thread();
        hub->kill_children();
        hub->release();
    }, hub);

    return true;
}

static bool found_usb_hub_device(usb_device& d, const usb_device_config& config,
                                 const usb_interface& interface) {
    if (interface.desc.bAlternateSetting)
        return false;

    return found_usb_hub_interface(d, config, interface);
}
