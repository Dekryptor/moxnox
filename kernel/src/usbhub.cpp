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
    usb_device_hub(usb_device& d, xhci_queue& queue, usb_hub_descriptor* hub_desc);
    ~usb_device_hub();
    void recv_thread();

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    bool initialized = false;

private:
    bool set_port_feature(enum usb2_port_feature_selector feature, uint16_t index);
    bool clear_port_feature(enum usb2_port_feature_selector feature,
                            uint16_t index);
    bool get_port_status(uint8_t port, port_status& status);
    void port_status_change(uint8_t port);
    void device_connected(uint8_t port);

    usb_device& d;
    xhci_queue& queue;
    usb_hub_descriptor* hub_desc = nullptr;
    usb_hub_port* ports = nullptr;
    atomic<unsigned int> refcount = 1;
};

static bool found_usb_hub_device(usb_device& d, uint8_t bInterfaceNumber,
                                 bool bAlternateSetting);

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

usb_device_hub::usb_device_hub(usb_device& d, xhci_queue& queue, usb_hub_descriptor* hub_desc) :
        usb_hub(d.controller, &d, d.parent.depth + 1), d(d), queue(queue) {
    this->hub_desc = hub_desc;

    d.acquire();

    ports = (usb_hub_port*)malloc(sizeof(usb_hub_port) * hub_desc->bNbrPorts);
    if (!ports) {
        print("malloc failed\n");
        return;
    }

    memset(ports, 0, sizeof(usb_hub_port) * hub_desc->bNbrPorts);

    for (uint8_t port = 1; port <= hub_desc->bNbrPorts; port++) {
        if (!set_port_feature(usb2_port_feature_selector::PORT_POWER, port)) {
            print("usb_device_hub::set_port_feature failed\n");
            return;
        }
    }

    for (uint8_t port = 1; port <= hub_desc->bNbrPorts; port++) {
        if (!set_port_feature(usb2_port_feature_selector::PORT_POWER, port)) {
            print("usb_device_hub::set_port_feature(PORT_POWER) failed\n");
            return;
        }
    }

    // FIXME - only if hub not USB 3?
    for (uint8_t port = 1; port <= hub_desc->bNbrPorts; port++) {
        if (!set_port_feature(usb2_port_feature_selector::PORT_RESET, port)) {
            print("usb_device_hub::set_port_feature(PORT_RESET) failed\n");
            return;
        }
    }

    initialized = true;
}

usb_device_hub::~usb_device_hub() {
    d.release();

    if (hub_desc)
        free(hub_desc);

    if (ports)
        free(ports);
}

void usb_device_hub::device_connected(uint8_t port) {
    print("usb_device_hub::device_connected: new device on port {}\n", port);

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
            } else {
                print("usb_device_hub::port_status_change: device removed from port {}\n", port);
                // FIXME - process
            }
        }

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
    size_t msg_size = (hub_desc->bNbrPorts / 8) + 1;

    auto buf = (uint8_t*)malloc(msg_size);
    if (!buf) {
        print("malloc failed\n");
        return;
    }

    while (true) {
        if (!queue.normal_transfer(span(buf, msg_size))) {
            print("xhci_queue::normal transfer failed\n");
            free(buf);
            return;
        }

        if (buf[0] & 1) {
            print("usb_device_hub::recv_thread: hub change detected\n");
            // FIXME - process and clear change bit (C_HUB_LOCAL_POWER or C_HUB_OVER_CURRENT)
        }

        for (uint8_t i = 0; i < hub_desc->bNbrPorts; i++) {
            if (buf[(i + 1) / 8] & (1 << ((i + 1) % 8)))
                port_status_change(i + 1);
        }
    }

    free(buf);
}

static usb_hub_descriptor* get_hub_descriptor(usb_device& d) {
    // FIXME - "enhanced" hubs

    size_t size = offsetof(usb_hub_descriptor, DeviceRemovable);

    auto desc = (usb_hub_descriptor*)malloc(size);
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

        desc = (usb_hub_descriptor*)malloc(size);
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

    print_usb_config(span((uint8_t*)desc, size));

    return desc;
}

static bool found_usb_hub_interface(usb_device& d, span<const uint8_t> buf) {
    auto& i = *(usb_interface_descriptor*)buf.data();

    if (i.bAlternateSetting)
        return false;

    usb_endpoint_descriptor* ep_desc = nullptr;

    // find endpoint number

    // skip INTERFACE
    buf = buf.subspan(i.bLength);

    while (!buf.empty()) {
        auto len = buf[0];

        if (len > buf.size() || len < 2) {
            print("buffer truncated\n");
            return false;
        }

        auto type = (enum usb_descriptor)buf[1];

        if (type == usb_descriptor::INTERFACE)
            break;

        switch (type) {
            case usb_descriptor::ENDPOINT: {
                auto& e = *(usb_endpoint_descriptor*)buf.data();

                if (len < sizeof(usb_endpoint_descriptor)) {
                    print("buffer truncated\n");
                    return false;
                }

                // look for periodic interrupt IN endpoint

                if (e.direction != USB_ENDPOINT_DIRECTION_IN)
                    break;

                if (e.transfer_type != USB_ENDPOINT_TRANSFER_TYPE_INTERRUPT)
                    break;

                if (ep_desc) {
                    print("found_usb_hub_interface: multiple potential endpoints found\n");
                    return false;
                }

                ep_desc = &e;

                break;
            }

            default:
                break;
        }

        buf = buf.subspan(len);
    }

    if (!ep_desc) {
        print("found_usb_hub_interface: no potential endpoint found\n");
        return false;
    }

    auto hub_desc = get_hub_descriptor(d);
    if (!hub_desc) {
        print("get_hub_descriptor failed\n");
        return false;
    }

    if (hub_desc->bNbrPorts == 0) {
        print("found_usb_hub_interface: number of ports was 0\n");
        free(hub_desc);
        return false;
    }

    if (!d.set_hub_ports(hub_desc->bNbrPorts)) {
        print("usb_device::set_hub_ports failed\n");
        free(hub_desc);
        return false;
    }

    auto queue = d.add_endpoint(*ep_desc, hub_desc->bNbrPorts);

    if (!queue) {
        print("usb_device::add_endpoint failed\n");
        free(hub_desc);
        return false;
    }

    auto hub = new usb_device_hub(d, *queue, hub_desc);
    if (!hub) {
        print("new failed\n");
        free(hub_desc);
        return false;
    }

    if (!hub->initialized) {
        print("usb_device_hub::usb_device_hub failed\n");
        delete hub;
        return false;
    }

    add_kernel_thread([](void* ctx) {
        ((usb_device_hub*)ctx)->recv_thread();
    }, hub);

    return true;
}

static bool found_usb_hub_device(usb_device& d, uint8_t bInterfaceNumber,
                                 bool bAlternateSetting) {
    auto buf = span((uint8_t*)d.config, d.config_size);

    if (bAlternateSetting)
        return false;

    if (d.device_descriptor.bcdUSB >= 0x300) {
        print("found_usb_hub_device: FIXME - support USB 3 hubs\n"); // FIXME
        return false;
    }

    while (!buf.empty()) {
        auto len = buf[0];

        if (len > buf.size() || len < 2) {
            print("buffer truncated\n");
            return false;
        }

        auto type = (enum usb_descriptor)buf[1];

        switch (type) {
            case usb_descriptor::INTERFACE: {
                auto& i = *(usb_interface_descriptor*)buf.data();

                if (len < sizeof(usb_interface_descriptor)) {
                    print("buffer truncated\n");
                    return false;
                }

                if (i.bInterfaceNumber == bInterfaceNumber)
                    return found_usb_hub_interface(d, buf);

                break;
            }

            default:
                break;
        }

        buf = buf.subspan(len);
    }

    return false;
}
