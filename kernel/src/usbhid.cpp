#include <atomic>
#include <string_view>
#include <span>
#include <mutex>
#include <optional>

import nt;
import print;
import format;
import xhci;
import usb;
import keyboard;
import pipe;
import evdev;
import timer;
import module_init;
import thread;
import mem;
import scheduler;
import list;
import spinlock;
import handle;
import object_ref;
import config;
import moxnox_ioctls;

using namespace std;

static const int64_t REPEAT_DELAY_MS = 660;
static const int64_t REPEAT_RATE_HZ = 25;

static const size_t evdev_buffer_pages = 1;

static atomic<unsigned int> keyboard_num = 0;

struct usb_hid_field {
    list_entry le;
    bool is_variable;
    bool is_relative;
    unsigned int count;
    unsigned int size;
    int32_t logical_min;
    int32_t logical_max;
    uint16_t usage_page;
    uint32_t usage_min;
    uint32_t usage_max;
    unsigned int bit_offset;
};

struct usb_hid_report {
    list_entry le;
    uint8_t id;
    list_entry fields = { &fields, &fields };
};

class usb_keyboard {
public:
    usb_keyboard(usb_device& d, const usb_device_config& config,
                 xhci_queue& queue, uint16_t packet_size,
                 const usb_interface& interface);
    ~usb_keyboard();
    void set_leds(keyboard_leds leds);
    void recv_thread();

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

private:
    void post_evdev(enum evdev_code code, int32_t value);
    void keyboard_repeat_thread();
    bool get_report_descriptor(const usb_device_config& cfg);
    bool parse_report_descriptor(span<const uint8_t> r);
    usb_hid_report* find_report(uint8_t id);

    usb_device& d;
    xhci_queue& queue;
    object_ref evdev_pipe;
    uint16_t packet_size;
    const usb_interface& interface;
    keyboard_context* kc = nullptr;
    enum evdev_code repeat_key = evdev_code::KEY_RESERVED;
    object_ref repeat_timer;
    atomic<unsigned int> refcount = 1;
    uint8_t* report_descriptor = nullptr;
    size_t report_descriptor_len;
    list_entry reports = { &reports, &reports };
};

static const enum evdev_code keycodes[] = {
    // 00
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_A,
    evdev_code::KEY_B,
    evdev_code::KEY_C,
    evdev_code::KEY_D,
    evdev_code::KEY_E,
    evdev_code::KEY_F,
    evdev_code::KEY_G,
    evdev_code::KEY_H,
    evdev_code::KEY_I,
    evdev_code::KEY_J,
    evdev_code::KEY_K,
    evdev_code::KEY_L,
    // 10
    evdev_code::KEY_M,
    evdev_code::KEY_N,
    evdev_code::KEY_O,
    evdev_code::KEY_P,
    evdev_code::KEY_Q,
    evdev_code::KEY_R,
    evdev_code::KEY_S,
    evdev_code::KEY_T,
    evdev_code::KEY_U,
    evdev_code::KEY_V,
    evdev_code::KEY_W,
    evdev_code::KEY_X,
    evdev_code::KEY_Y,
    evdev_code::KEY_Z,
    evdev_code::KEY_1,
    evdev_code::KEY_2,
    // 20
    evdev_code::KEY_3,
    evdev_code::KEY_4,
    evdev_code::KEY_5,
    evdev_code::KEY_6,
    evdev_code::KEY_7,
    evdev_code::KEY_8,
    evdev_code::KEY_9,
    evdev_code::KEY_0,
    evdev_code::KEY_ENTER,
    evdev_code::KEY_ESC,
    evdev_code::KEY_BACKSPACE,
    evdev_code::KEY_TAB,
    evdev_code::KEY_SPACE,
    evdev_code::KEY_MINUS,
    evdev_code::KEY_EQUAL,
    evdev_code::KEY_LEFTBRACE,
    // 30
    evdev_code::KEY_RIGHTBRACE,
    evdev_code::KEY_BACKSLASH, // qemu usb-kbd uses this rather than the following?
    evdev_code::KEY_BACKSLASH,
    evdev_code::KEY_SEMICOLON,
    evdev_code::KEY_APOSTROPHE,
    evdev_code::KEY_GRAVE,
    evdev_code::KEY_COMMA,
    evdev_code::KEY_DOT,
    evdev_code::KEY_SLASH,
    evdev_code::KEY_CAPSLOCK,
    evdev_code::KEY_F1,
    evdev_code::KEY_F2,
    evdev_code::KEY_F3,
    evdev_code::KEY_F4,
    evdev_code::KEY_F5,
    evdev_code::KEY_F6,
    // 40
    evdev_code::KEY_F7,
    evdev_code::KEY_F8,
    evdev_code::KEY_F9,
    evdev_code::KEY_F10,
    evdev_code::KEY_F11,
    evdev_code::KEY_F12,
    evdev_code::KEY_SYSRQ,
    evdev_code::KEY_SCROLLLOCK,
    evdev_code::KEY_PAUSE,
    evdev_code::KEY_INSERT,
    evdev_code::KEY_HOME,
    evdev_code::KEY_PAGEUP,
    evdev_code::KEY_DELETE,
    evdev_code::KEY_END,
    evdev_code::KEY_PAGEDOWN,
    evdev_code::KEY_RIGHT,
    // 50
    evdev_code::KEY_LEFT,
    evdev_code::KEY_DOWN,
    evdev_code::KEY_UP,
    evdev_code::KEY_NUMLOCK,
    evdev_code::KEY_KPSLASH,
    evdev_code::KEY_KPASTERISK,
    evdev_code::KEY_KPMINUS,
    evdev_code::KEY_KPPLUS,
    evdev_code::KEY_KPENTER,
    evdev_code::KEY_KP1,
    evdev_code::KEY_KP2,
    evdev_code::KEY_KP3,
    evdev_code::KEY_KP4,
    evdev_code::KEY_KP5,
    evdev_code::KEY_KP6,
    evdev_code::KEY_KP7,
    // 60
    evdev_code::KEY_KP8,
    evdev_code::KEY_KP9,
    evdev_code::KEY_KP0,
    evdev_code::KEY_KPDOT,
    evdev_code::KEY_102ND,
    evdev_code::KEY_COMPOSE,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    // 70
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    // 80
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    // 90
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_SYSRQ,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    // a0
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    // b0
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    // c0
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    // d0
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    // e0
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    // f0
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
    evdev_code::KEY_RESERVED,
};

static bool found_usb_hid_device(usb_device& d, const usb_device_config& config,
                                 const usb_interface& interface);

static usb_recognizer mod_usb_interfaces = {
    {}, usb_class_code::hid, found_usb_hid_device
};

static void init_usbhid() {
    insert_tail_list(usb_interfaces, mod_usb_interfaces.le);
}

static module_init module_init_usbhid(init_usbhid);

enum class usb_hid_request : uint8_t {
    GET_REPORT = 0x01,
    GET_IDLE = 0x02,
    GET_PROTOCOL = 0x03,
    SET_REPORT = 0x09,
    SET_IDLE = 0x0a,
    SET_PROTOCOL = 0x0b,
};

enum class usb_hid_report_type : uint8_t {
    INPUT = 0x01,
    OUTPUT = 0x02,
    FEATURE = 0x03,
};

static bool usb_hid_set_report(usb_device& d, span<uint8_t> buf,
                               enum usb_hid_report_type report_type,
                               uint8_t report_id, uint16_t interface) {
    return d.ep0_queue.control_transfer(buf,
                                        USB_REQUEST_TYPE_HOST_TO_DEVICE | USB_REQUEST_TYPE_CLASS | USB_REQUEST_TYPE_RECIPIENT_INTERFACE,
                                        (enum usb_request)usb_hid_request::SET_REPORT,
                                        (uint16_t)(((uint8_t)report_type << 8) | report_id),
                                        interface);
}

bool usb_keyboard::get_report_descriptor(const usb_device_config& cfg) {
    auto sp = span(cfg.data, cfg.len);
    span<const uint8_t> hid;
    bool ignore = true;

    while (true) {
        if (sp.empty())
            return false;

        auto len = sp[0];

        if (sp.size() < len || len < 2)
            return false;

        auto type = (usb_descriptor)sp[1];

        if (type == usb_descriptor::INTERFACE && len >= sizeof(usb_interface_descriptor)) {
            auto& desc = *(usb_interface_descriptor*)sp.data();

            ignore = desc.bInterfaceNumber != interface.desc.bInterfaceNumber;
        }

        if (type == usb_descriptor::HID && !ignore) {
            hid = sp.subspan(0, len);
            break;
        }

        sp = sp.subspan(len);
    }

    if (hid.empty()) {
        print("HID descriptor not found for interface {} on USB HID device\n",
              interface.desc.bInterfaceNumber);
        return false;
    }

    if (hid.size() < offsetof(usb_hid_descriptor, entries)) {
        print("HID descriptor was {} bytes, expected at least {}\n", hid.size(),
              offsetof(usb_hid_descriptor, entries));
        return false;
    }

    auto& h = *(usb_hid_descriptor*)hid.data();

    if (hid.size() < offsetof(usb_hid_descriptor, entries) + (sizeof(usb_hid_descriptor_entry) * h.bNumDescriptors)) {
        print("HID descriptor was {} bytes, expected {}\n", hid.size(),
              offsetof(usb_hid_descriptor, entries), offsetof(usb_hid_descriptor, entries) + (sizeof(usb_hid_descriptor_entry) * h.bNumDescriptors));
        return false;
    }

    optional<uint16_t> len;

    for (unsigned int i = 0; i < h.bNumDescriptors; i++) {
        if (h.entries[i].bDescriptorType != usb_descriptor::HID_REPORT)
            continue;

        len = +h.entries[i].wDescriptorLength;
        break;
    }

    if (!len.has_value()) {
        print("HID report descriptor not found for interface {} on USB HID device\n",
              interface.desc.bInterfaceNumber);
        return false;
    }

    if (*len == 0) {
        print("HID report descriptor length was 0\n");
        return false;
    }

    report_descriptor_len = *len;

    report_descriptor = (uint8_t*)malloc(*len);
    if (!report_descriptor) {
        print("malloc failed\n");
        return false;
    }

    if (!d.get_descriptor_interface(span(report_descriptor, *len),
                                    usb_descriptor::HID_REPORT, 0,
                                    interface.desc.bInterfaceNumber)) {
        print("usb_device::get_descriptor_interface failed\n");
        return false;
    }

    return true;
}

static int32_t get_signed_num(uint32_t v, unsigned int n) {
    if (n == 1 && v & 0x80)
        return (int32_t)(0xffffff00 | (uint8_t)v);
    else if (n == 2 && v & 0x8000)
        return (int32_t)(0xffff0000 | (uint16_t)v);
    else if (n == 4 && v & 0x80000000)
        return (int32_t)v;
    else
        return v;
}

#if 0
static void print_report_descriptor(span<const uint8_t> r) {
    print("report descriptor:\n");

    while (!r.empty()) {
        auto b = r[0];

        if (b == 0xfe) { // long item
            if (r.size() == 1) {
                print("report descriptor is truncated\n");
                return;
            }

            uint8_t size = r[1];

            if (r.size() < (size_t)(size + 3)) {
                print("report descriptor is truncated\n");
                return;
            }

            print("parse_report_descriptor: skipping long item\n");
            r = r.subspan(size + 3);
            continue;
        }

        unsigned int n;

        switch (b & 0x3) {
            case 0:
                n = 0;
                break;

            case 1:
                n = 1;
                break;

            case 2:
                n = 2;
                break;

            case 3:
                n = 4;
                break;
        }

        if (r.size() < n + 1) {
            print("report descriptor is truncated\n");
            return;
        }

        auto sp = span(r.data() + 1, n);

        uint32_t v;

        if (n == 0)
            v = 0;
        else if (n == 1)
            v = sp[0];
        else if (n == 2)
            v = (sp[1] << 8) | sp[0];
        else if (n == 4)
            v = (sp[3] << 24) | (sp[2] << 16) | (sp[1] << 8) | sp[0];

        switch (b & 0xc) {
            case 0x0: // main
                switch (b & 0xf0) {
                    case 0x80: {
                        print("input {:x}\n", v);
                        break;
                    }

                    case 0x90: {
                        print("output {:x}\n", v);
                        break;
                    }

                    case 0xa0: {
                        print("collection {:x}\n", v);
                        break;
                    }

                    case 0xb0: {
                        print("feature {:x}\n", v);
                        break;
                    }

                    case 0xc0:
                        print("end collection\n");
                        break;

                    default:
                        print("unrecognized main item {:02x}, {:x}\n", b, v);
                        break;
                }
            break;

            case 0x4: // global
                switch (b & 0xf0) {
                    case 0x00:
                        print("usage page {:x}\n", v);
                        break;

                    case 0x10:
                        print("logical minimum {}\n", get_signed_num(v, n));
                        break;

                    case 0x20:
                        print("logical maximum {}\n", get_signed_num(v, n));
                        break;

                    case 0x30:
                        print("physical minimum {}\n", get_signed_num(v, n));
                        break;

                    case 0x40:
                        print("physical maximum {}\n", get_signed_num(v, n));
                        break;

                    case 0x50:
                        print("unit exponent {:x}\n", v);
                        break;

                    case 0x60:
                        print("unit {:x}\n", v);
                        break;

                    case 0x70:
                        print("report size {:x}\n", v);
                        break;

                    case 0x80:
                        print("report ID {:x}\n", v);
                        break;

                    case 0x90:
                        print("report count {:x}\n", v);
                        break;

                    case 0xa0:
                        print("push\n");
                        break;

                    case 0xb0:
                        print("pop\n");
                        break;

                    default:
                        print("unrecognized global item {:02x}, {:x}\n", b, v);
                        break;
                }
            break;

            case 0x8: // local
                switch (b & 0xf0) {
                    case 0x00:
                        print("usage {:x}\n", v);
                        break;

                    case 0x10:
                        print("usage minimum {:x}\n", v);
                        break;

                    case 0x20:
                        print("usage maximum {:x}\n", v);
                        break;

                    case 0x30:
                        print("designator index {:x}\n", v);
                        break;

                    case 0x40:
                        print("designator minimum {:x}\n", v);
                        break;

                    case 0x50:
                        print("designator maximum {:x}\n", v);
                        break;

                    case 0x70:
                        print("string index {:x}\n", v);
                        break;

                    case 0x80:
                        print("string minimum {:x}\n", v);
                        break;

                    case 0x90:
                        print("string maximum {:x}\n", v);
                        break;

                    case 0xa0:
                        print("delimiter {:x}\n", v);
                        break;

                    default:
                        print("unrecognized local item {:02x}, {:x}\n", b, v);
                        break;
                }
            break;

            default:
                print("unrecognized item {:02x}, {:x}\n", b, v);
                break;
        }

        r = r.subspan(n + 1);
    }
}
#endif

usb_hid_report* usb_keyboard::find_report(uint8_t id) {
    for (auto le = reports.next; le != &reports; le = le->next) {
        auto& r = containing_record<usb_hid_report>(le);

        if (r.id == id)
            return &r;
    }

    auto r = new usb_hid_report;
    if (!r) {
        print("new failed\n");
        return nullptr;
    }

    r->id = id;

    insert_tail_list(reports, r->le);

    return r;
}

bool usb_keyboard::parse_report_descriptor(span<const uint8_t> r) {
    uint32_t usage_min = 0, usage_max = 0;
    uint8_t report_id = 0;
    uint32_t report_count = 0, report_size = 0;
    uint16_t usage_page = 0;
    uint32_t logical_max_u = 0;
    int32_t logical_min = 0, logical_max_s = 0;
    unsigned int input_bit_offset = 0, output_bit_offset = 0;

    print("report descriptor:\n");

    while (!r.empty()) {
        auto b = r[0];

        if (b == 0xfe) { // long item
            if (r.size() == 1) {
                print("report descriptor is truncated\n");
                return false;
            }

            uint8_t size = r[1];

            if (r.size() < (size_t)(size + 3)) {
                print("report descriptor is truncated\n");
                return false;
            }

            r = r.subspan(size + 3);
            continue;
        }

        unsigned int n;

        switch (b & 0x3) {
            case 0:
                n = 0;
                break;

            case 1:
                n = 1;
                break;

            case 2:
                n = 2;
                break;

            case 3:
                n = 4;
                break;
        }

        if (r.size() < n + 1) {
            print("report descriptor is truncated\n");
            return false;
        }

        auto sp = span(r.data() + 1, n);

        uint32_t v;

        if (n == 0)
            v = 0;
        else if (n == 1)
            v = sp[0];
        else if (n == 2)
            v = (sp[1] << 8) | sp[0];
        else if (n == 4)
            v = (sp[3] << 24) | (sp[2] << 16) | (sp[1] << 8) | sp[0];

        switch (b & 0xc) {
            case 0x0: // main
                switch (b & 0xf0) {
                    case 0x80: {
                        if (!(v & 1)) { // skip constant
                            int32_t logical_max;

                            auto rep = find_report(report_id);
                            if (!rep)
                                return false;

                            if (logical_min >= 0)
                                logical_max = logical_max_u;
                            else
                                logical_max = logical_max_s;

                            auto f = new usb_hid_field;
                            if (!f) {
                                print("new failed\n");
                                return false;
                            }

                            f->is_variable = v & 2;
                            f->is_relative = v & 4;
                            f->count = report_count;
                            f->size = report_size;
                            f->logical_min = logical_min;
                            f->logical_max = logical_max;
                            f->usage_page = usage_page;
                            f->usage_min = usage_min;
                            f->usage_max = usage_max;
                            f->bit_offset = input_bit_offset;

                            insert_tail_list(rep->fields, f->le);
                        }

                        usage_min = 0;
                        usage_max = 0;

                        input_bit_offset += report_count * report_size;

                        break;
                    }

                    case 0x90: {
                        if (!(v & 1)) { // skip constant
                            int32_t logical_max;

                            if (logical_min >= 0)
                                logical_max = logical_max_u;
                            else
                                logical_max = logical_max_s;

                            print("output {:x} (usage min = {:x}, usage max = {:x}, report ID {:x}, report count {:x}, report size {:x}, usage page {:x}, logical min = {}, logical max = {}, bit offset = {})\n",
                                v, usage_min, usage_max, report_id, report_count,
                                report_size, usage_page, logical_min, logical_max,
                                output_bit_offset);
                        }

                        usage_min = 0;
                        usage_max = 0;

                        output_bit_offset += report_count * report_size;

                        break;
                    }

                    default:
                        usage_min = 0;
                        usage_max = 0;
                        break;
                }
            break;

            case 0x4: // global
                switch (b & 0xf0) {
                    case 0x00:
                        // FIXME - return error if more than 0xffff?
                        usage_page = (uint16_t)v;
                        break;

                    case 0x10:
                        logical_min = get_signed_num(v, n);
                        break;

                    case 0x20:
                        logical_max_u = v;
                        logical_max_s = get_signed_num(v, n);
                        break;

                    case 0x70:
                        report_size = v;
                        break;

                    case 0x80:
                        // FIXME - return error if more than 0xff?
                        report_id = (uint8_t)v;
                        break;

                    case 0x90:
                        report_count = v;
                        break;

                    default:
                        break;
                }
            break;

            case 0x8: // local
                switch (b & 0xf0) {
                    case 0x00:
                        usage_min = v;
                        usage_max = v;
                        break;

                    case 0x10:
                        usage_min = v;
                        break;

                    case 0x20:
                        usage_max = v;
                        break;

                    default:
                        break;
                }
            break;

            default:
                break;
        }

        r = r.subspan(n + 1);
    }

    return true;
}

usb_keyboard::usb_keyboard(usb_device& d, const usb_device_config& config,
                           xhci_queue& queue, uint16_t packet_size,
                           const usb_interface& interface) : d(d), queue(queue), interface(interface) {
    d.acquire();

    if (!get_report_descriptor(config)) {
        print("usb_keyboard::get_report_descriptor failed\n");
        return;
    }

#if 0
    print_report_descriptor(span(report_descriptor, report_descriptor_len));
#endif
    parse_report_descriptor(span(report_descriptor, report_descriptor_len));

    for (auto le = reports.next; le != &reports; le = le->next) {
        auto& r = containing_record<usb_hid_report>(le);

        print("report {}:\n", r.id);

        for (auto le2 = r.fields.next; le2 != &r.fields; le2 = le2->next) {
            auto& f = containing_record<usb_hid_field>(le2);

            print("input (is_variable {}, is_relative {}, usage min = {:x}, usage max = {:x}, count {:x}, size {:x}, usage page {:x}, logical min = {}, logical max = {}, bit offset = {})\n",
                  f.is_variable ? "true" : "false", f.is_relative ? "true" : "false",
                  f.usage_min, f.usage_max, f.count, f.size,
                  f.usage_page, f.logical_min, f.logical_max, f.bit_offset);
        }
    }

    this->packet_size = packet_size;

    auto pptr = new pipe(evdev_buffer_pages);
    if (!pptr) {
        print("new failed\n");
        return;
    }

    evdev_pipe = object_ref{*pptr};

    unsigned int num = keyboard_num.fetch_add(1);

    {
        char s[50];
        auto sp = span(s, sizeof(s));

        sp = format(sp, "usbkeyboard{}", num);

        auto sv = string_view(s, (size_t)(sp.data() - s));

        // FIXME - put in /dev/input like on Linux?
        auto status = name_object(evdev_pipe.get(), sv);
        if (!success(status)) {
            print("name_object returned {}\n", status);
            return;
        }
    }

    this->kc = register_keyboard(static_cast<pipe&>(evdev_pipe.get()), [](void* ctx, keyboard_leds leds) {
        auto& kbd = *(usb_keyboard*)ctx;

        kbd.set_leds(leds);
    }, this);

    repeat_timer = create_timer(true);
    if (!repeat_timer) {
        print("create_timer failed\n");
        return;
    }

    acquire();
    if (!add_kernel_thread([](void* ctx) { ((usb_keyboard*)ctx)->keyboard_repeat_thread(); }, this)) {
        release();
        print("add_kernel_thread failed\n");
        return;
    }
}

usb_keyboard::~usb_keyboard() {
    if (kc)
        kc->unregister();

    auto& p = static_cast<pipe&>(evdev_pipe.get());

    {
        sync_lock sl;
        lock_guard l(sl);

        if (!p.signalled) {
            p.signalled = true;

            wake_up_waiters(p.waiters);
        }
    }

    if (report_descriptor)
        free(report_descriptor);

    while (!is_list_empty(reports)) {
        auto& r = containing_record<usb_hid_report>(reports.next);

        remove_entry_list(r.le);

        while (!is_list_empty(r.fields)) {
            auto& f = containing_record<usb_hid_field>(r.fields.next);

            remove_entry_list(f.le);

            delete &f;
        }

        delete &r;
    }

    d.release();
}

void usb_keyboard::set_leds(keyboard_leds leds) {
    uint8_t buf = 0;

    if (leds.num_lock)
        buf |= 1 << 0;

    if (leds.caps_lock)
        buf |= 1 << 1;

    if (leds.scroll_lock)
        buf |= 1 << 2;

    if (!usb_hid_set_report(d, span(&buf, 1), usb_hid_report_type::OUTPUT, 0,
                            interface.desc.bInterfaceNumber)) {
        print("usb_hid_set_report failed\n");
        return;
    }
}

static bool should_key_repeat(enum evdev_code code) {
    switch (code) {
        case evdev_code::KEY_RESERVED:
        case evdev_code::KEY_LEFTALT:
        case evdev_code::KEY_RIGHTALT:
        case evdev_code::KEY_LEFTCTRL:
        case evdev_code::KEY_RIGHTCTRL:
        case evdev_code::KEY_LEFTSHIFT:
        case evdev_code::KEY_RIGHTSHIFT:
        case evdev_code::KEY_LEFTMETA:
        case evdev_code::KEY_RIGHTMETA:
        case evdev_code::KEY_CAPSLOCK:
        case evdev_code::KEY_NUMLOCK:
        case evdev_code::KEY_SCROLLLOCK:
            return false;

        default:
            return true;
    }
}

void usb_keyboard::post_evdev(enum evdev_code code, int32_t value) {
    input_event ev;
    auto& p = static_cast<pipe&>(evdev_pipe.get());

    if (value != 2 && should_key_repeat(code)) {
        auto& t = static_cast<timer&>(repeat_timer.get());

        if (value == 0) { // up
            if (repeat_key == code) {
                if (auto status = t.cancel(nullptr); !success(status))
                    print("timer::cancel returned {}\n", status);

                repeat_key = evdev_code::KEY_RESERVED;
            }
        } else { // down
            if (repeat_key == evdev_code::KEY_RESERVED) {
                if (auto status = t.set(-ONE_MS_TICKS * REPEAT_DELAY_MS, nullptr,
                                        nullptr, false, 0, nullptr); !success(status)) {
                    print("timer::set returned {}\n", status);
                }
            } else if (repeat_key != code) {
                if (auto status = t.cancel(nullptr); !success(status))
                    print("timer::cancel returned {}\n", status);

                if (auto status = t.set(-ONE_MS_TICKS * REPEAT_DELAY_MS, nullptr,
                                        nullptr, false, 0, nullptr); !success(status)) {
                    print("timer::set returned {}\n", status);
                }
            }

            repeat_key = code;
        }
    }

    // FIXME - Linux also has EV_SYN and EV_MSC events

    // FIXME - populate time field
    ev.time.tv_sec = 0;
    ev.time.tv_usec = 0;

    ev.type = evdev_type::EV_KEY;
    ev.code = code;
    ev.value = value;

    auto sp = span((uint8_t*)&ev, sizeof(ev));

    while (!sp.empty()) {
        uint64_t written_bytes;

        auto status = p.write(sp, written_bytes);
        if (!success(status)) {
            print("pipe::write returned {}\n", status);
            return;
        }

        if (written_bytes == sp.size())
            break;

        sp = span(sp.data() + written_bytes, sp.size() - written_bytes);
    }
}

void usb_keyboard::recv_thread() {
    if (packet_size < 8) {
        print("usb_keyboard::recv_thread: packet_size was {}, expected at least 8\n",
              packet_size);
        return;
    }

    auto buf = (uint8_t*)malloc(packet_size);
    if (!buf) {
        print("malloc failed\n");
        return;
    }

    uint64_t status[4], last_status[4];
    uint8_t last_mod = 0;

    for (unsigned int i = 0; i < 4; i++) {
        last_status[i] = 0;
    }

    while (true) {
        auto recv_bytes = queue.normal_transfer(span(buf, packet_size));

        if (recv_bytes == 0) {
            if (!d.dead)
                print("xhci_queue::normal_transfer failed\n");
            break;
        }

        if (recv_bytes < 2) {
            print("usb_keyboard::recv_thread: received {} bytes, expected at least 2\n",
                  recv_bytes);
            continue;
        }

#if 0
        char s[255];
        span<char> sp(s, sizeof(s));

        for (uint16_t i = 0; i < recv_bytes; i++) {
            if (i != 0)
                sp = format_to(sp, " ", make_tuple());

            sp = format_to(sp, "{:02x}", make_tuple(buf[i]));
        }

        print("{}\n", string_view(s, (size_t)(sp.data() - s)));
#endif
        uint8_t mod = buf[0];

        for (unsigned int i = 0; i < 4; i++) {
            status[i] = 0;
        }

        for (uint16_t i = 2; i < recv_bytes; i++) {
            if (buf[i] == 0)
                continue;

            status[buf[i] / 64] |= (uint64_t)1 << (buf[i] % 64);
        }

        if (mod != last_mod) {
            uint8_t changed = mod ^ last_mod;

            while (changed != 0) {
                auto bit = countr_zero(changed);
                bool down = mod & (1 << bit);

                static const enum evdev_code modifiers[] = {
                    evdev_code::KEY_LEFTCTRL,
                    evdev_code::KEY_LEFTSHIFT,
                    evdev_code::KEY_LEFTALT,
                    evdev_code::KEY_LEFTMETA,
                    evdev_code::KEY_RIGHTCTRL,
                    evdev_code::KEY_RIGHTSHIFT,
                    evdev_code::KEY_RIGHTALT,
                    evdev_code::KEY_RIGHTMETA,
                };

                post_evdev(modifiers[bit], down ? 1 : 0);

                changed &= (uint8_t)~(1 << bit);
            }
        }

        for (unsigned int i = 0; i < 4; i++) {
            if (status[i] == last_status[i])
                continue;

            auto changed = status[i] ^ last_status[i];

            while (changed != 0) {
                auto bit = (unsigned int)countr_zero(changed);
                auto code = (i * 64) + bit;
                bool down = status[i] & (uint64_t)1 << bit;

                if (keycodes[code] != evdev_code::KEY_RESERVED)
                    post_evdev(keycodes[code], down ? 1 : 0);
                else
                    print("unrecognized keycode {:x}\n", code);

                changed &= ~((uint64_t)1 << bit);
            }
        }

        for (unsigned int i = 0; i < 4; i++) {
            last_status[i] = status[i];
        }

        last_mod = mod;
    }

    free(buf);

    if (repeat_timer) {
        auto& t = static_cast<timer&>(repeat_timer.get());

        if (auto status = t.set(0, nullptr, nullptr, false, 0, nullptr); !success(status))
            print("timer::set returned {}\n", status);
    }
}

static bool set_protocol(usb_device& d, uint16_t wValue, uint16_t wIndex) {
    if (!d.ep0_queue.control_transfer(span<uint8_t>(),
            USB_REQUEST_TYPE_HOST_TO_DEVICE | USB_REQUEST_TYPE_CLASS | USB_REQUEST_TYPE_RECIPIENT_INTERFACE,
            (enum usb_request)usb_hid_request::SET_PROTOCOL,
            wValue, wIndex)) {
        print("xhci_queue::control_transfer failed\n");
        return false;
    }

    return true;
}

void usb_keyboard::keyboard_repeat_thread() {
    auto& t = static_cast<timer&>(repeat_timer.get());

    t.acquire();

    while (true) {
        auto status = wait_for_object(repeat_timer.get(), false);
        if (!success(status)) {
            print("wait_for_object returned {}\n", status);
            break;
        }

        if (d.dead)
            break;

        auto code = repeat_key;

        if (code == evdev_code::KEY_RESERVED)
            continue;

        post_evdev(code, 2);

        auto& t = static_cast<timer&>(repeat_timer.get());

        if (auto status = t.set(-(SYSTEM_TIME_RESOLUTION / REPEAT_RATE_HZ), nullptr,
                                nullptr, false, 0, nullptr); !success(status)) {
            print("timer::set returned {}\n", status);
            break;
        }
    }

    t.release();
    release();
}

static bool found_usb_keyboard(usb_device& d, const usb_device_config& config,
                               const usb_interface& interface) {
    usb_endpoint* ep_desc = nullptr;

    // find endpoint number

    for (auto le = interface.endpoints.next; le != &interface.endpoints; le = le->next) {
        auto& e = containing_record<usb_endpoint>(le);

        // look for periodic interrupt IN endpoint

        if (e.desc.direction != USB_ENDPOINT_DIRECTION_IN)
            continue;

        if (e.desc.transfer_type != USB_ENDPOINT_TRANSFER_TYPE_INTERRUPT)
            continue;

        if (e.desc.usage_type != USB_ENDPOINT_USAGE_TYPE_PERIODIC)
            continue;

        if (ep_desc) {
            print("found_usb_keyboard: multiple potential endpoints found\n");
            return false;
        }

        ep_desc = &e;
    }

    if (!ep_desc) {
        print("found_usb_keyboard: no potential endpoint found\n");
        return false;
    }

    if (ep_desc->desc.wMaxPacketSize == 0) {
        print("found_usb_keyboard: wMaxPacketSize was 0\n");
        return false;
    }

    // change to boot protocol
    if (!set_protocol(d, 0, interface.desc.bInterfaceNumber)) {
        print("found_usb_keyboard: set_protocol failed\n");
        return false;
    }

    auto queue = d.configure_endpoint(ep_desc->desc);
    if (!queue) {
        print("usb_device::configure_endpoint failed\n");
        return false;
    }

    auto kbd = new usb_keyboard(d, config, *queue,
                                ep_desc->desc.wMaxPacketSize, interface);
    if (!kbd) {
        print("new failed\n");
        return false;
    }

    add_kernel_thread([](void* ctx) {
        auto kbd = (usb_keyboard*)ctx;

        kbd->recv_thread();
        kbd->release();
    }, kbd);

    return true;
}

static bool found_usb_hid_device(usb_device& d, const usb_device_config& config,
                                 const usb_interface& interface) {
    if (interface.desc.bAlternateSetting) // FIXME?
        return false;

    // FIXME - reject anything for now that doesn't have a keyboard boot interface
    if (interface.desc.bInterfaceSubClass != 1 || interface.desc.bInterfaceProtocol != 1)
        return false;

    return found_usb_keyboard(d, config, interface);
}
