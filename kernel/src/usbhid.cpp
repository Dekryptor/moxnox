#include <atomic>
#include <string_view>
#include <span>
#include <mutex>
#include <optional>
#include <string.h>

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

static const uint16_t USAGE_PAGE_KEYBOARD = 7;

static const size_t evdev_buffer_pages = 1;

static atomic<unsigned int> hid_num = 0;

struct usb_hid_field {
    ~usb_hid_field() {
        if (usages)
            free(usages);

        if (old)
            free(old);
    }

    list_entry le;
    bool is_variable;
    bool is_relative;
    unsigned int count;
    unsigned int size;
    int32_t logical_min;
    int32_t logical_max;
    uint16_t usage_page;
    size_t num_usages = 0;
    uint32_t* usages = nullptr;
    uint32_t usage_min;
    uint32_t usage_max;
    unsigned int bit_offset;
    void* old = nullptr;
};

struct usb_hid_report {
    list_entry le;
    uint8_t id;
    unsigned int bit_offset = 0;
    list_entry fields = { &fields, &fields };
};

class usb_hid_device {
public:
    usb_hid_device(usb_device& d, const usb_device_config& config,
                   xhci_queue& queue, uint16_t packet_size,
                   const usb_interface& interface);
    ~usb_hid_device();
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
    usb_hid_report* find_report(uint8_t id, bool input);
    void handle_input(usb_hid_report& r, span<const uint8_t> sp);
    void handle_input_field(usb_hid_field& f, span<const uint8_t> sp);
    void input_usage(uint16_t usage_page, uint32_t usage, int32_t num);

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
    bool is_keyboard = false;
    list_entry input_reports = { &input_reports, &input_reports };
    list_entry output_reports = { &output_reports, &output_reports };
    array<uint64_t, 4> keys;
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
    evdev_code::KEY_LEFTCTRL,
    evdev_code::KEY_LEFTSHIFT,
    evdev_code::KEY_LEFTALT,
    evdev_code::KEY_LEFTMETA,
    evdev_code::KEY_RIGHTCTRL,
    evdev_code::KEY_RIGHTSHIFT,
    evdev_code::KEY_RIGHTALT,
    evdev_code::KEY_RIGHTMETA,
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

bool usb_hid_device::get_report_descriptor(const usb_device_config& cfg) {
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
    else
        return (int32_t)v;
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

usb_hid_report* usb_hid_device::find_report(uint8_t id, bool input) {
    auto& list = input ? input_reports : output_reports;

    for (auto le = list.next; le != &list; le = le->next) {
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

    insert_tail_list(list, r->le);

    return r;
}

bool usb_hid_device::parse_report_descriptor(span<const uint8_t> r) {
    uint32_t usage_min = 0, usage_max = 0;
    size_t num_usages = 0;
    uint32_t* usages = nullptr;
    uint8_t report_id = 0;
    uint32_t report_count = 0, report_size = 0;
    uint16_t usage_page = 0;
    uint32_t logical_max_u = 0;
    int32_t logical_min = 0, logical_max_s = 0;

    while (!r.empty()) {
        auto b = r[0];

        if (b == 0xfe) { // long item
            if (r.size() == 1) {
                print("report descriptor is truncated\n");

                if (usages)
                    free(usages);

                return false;
            }

            uint8_t size = r[1];

            if (r.size() < (size_t)(size + 3)) {
                print("report descriptor is truncated\n");

                if (usages)
                    free(usages);

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

            if (usages)
                free(usages);

            return false;
        }

        auto sp = span(r.data() + 1, n);

        uint32_t v;

        if (n == 0)
            v = 0;
        else if (n == 1)
            v = sp[0];
        else if (n == 2)
            v = (uint32_t)(sp[1] << 8) | sp[0];
        else if (n == 4)
            v = (uint32_t)(sp[3] << 24) | (uint32_t)(sp[2] << 16) | (uint32_t)(sp[1] << 8) | sp[0];

        switch (b & 0xc) {
            case 0x0: // main
                switch (b & 0xf0) {
                    case 0x80: // input
                    case 0x90: // output
                    {
                        auto rep = find_report(report_id, (b & 0xf0) == 0x80);
                        if (!rep) {
                            if (usages)
                                free(usages);

                            return false;
                        }

                        if (!(v & 1)) { // skip constant
                            int32_t logical_max;

                            if (logical_min >= 0)
                                logical_max = (int32_t)logical_max_u;
                            else
                                logical_max = logical_max_s;

                            auto f = new usb_hid_field;
                            if (!f) {
                                print("new failed\n");

                                if (usages)
                                    free(usages);

                                return false;
                            }

                            f->is_variable = v & 2;
                            f->is_relative = v & 4;
                            f->count = report_count;
                            f->size = report_size;
                            f->logical_min = logical_min;
                            f->logical_max = logical_max;
                            f->usage_page = usage_page;
                            f->usages = usages;
                            f->num_usages = num_usages;
                            f->usage_min = usage_min;
                            f->usage_max = usage_max;
                            f->bit_offset = rep->bit_offset;

                            insert_tail_list(rep->fields, f->le);

                            usages = nullptr;
                        } else {
                            if (usages) {
                                free(usages);
                                usages = nullptr;
                            }
                        }

                        num_usages = 0;
                        usage_min = 0;
                        usage_max = 0;

                        rep->bit_offset += report_count * report_size;

                        break;
                    }

                    default:
                        usage_min = 0;
                        usage_max = 0;

                        if (usages) {
                            free(usages);
                            usages = nullptr;
                        }

                        num_usages = 0;

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
                        usages = (decltype(usages))realloc(usages, sizeof(*usages) + (num_usages + 1));
                        if (!usages) {
                            print("realloc failed\n");
                            return false;
                        }

                        usages[num_usages] = v;
                        num_usages++;

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

    if (usages)
        free(usages);

    return true;
}

static void print_field(const usb_hid_field& f) {
    print("is_variable {}, is_relative {}, ", f.is_variable ? "true" : "false",
          f.is_relative ? "true" : "false");

    if (f.num_usages == 0)
        print("usage min = {:x}, usage max = {:x}, ", f.usage_min, f.usage_max);
    else {
        print("usage = ");

        for (size_t i = 0; i < f.num_usages; i++) {
            print("{:x}, ", f.usages[i]);
        }
    }

    print("count {:x}, size {:x}, usage page {:x}, logical min = {}, logical max = {}, bit offset = {}\n",
          f.count, f.size, f.usage_page, f.logical_min, f.logical_max,
          f.bit_offset);
}

usb_hid_device::usb_hid_device(usb_device& d, const usb_device_config& config,
                               xhci_queue& queue, uint16_t packet_size,
                               const usb_interface& interface) : d(d), queue(queue), interface(interface) {
    d.acquire();

    if (!get_report_descriptor(config)) {
        print("usb_hid_device::get_report_descriptor failed\n");
        return;
    }

#if 0
    print_report_descriptor(span(report_descriptor, report_descriptor_len));
#endif
    parse_report_descriptor(span(report_descriptor, report_descriptor_len));

    for (auto le = input_reports.next; le != &input_reports; le = le->next) {
        auto& r = containing_record<usb_hid_report>(le);

        print("input report {}:\n", r.id);

        for (auto le2 = r.fields.next; le2 != &r.fields; le2 = le2->next) {
            auto& f = containing_record<usb_hid_field>(le2);

            if (f.usage_page == USAGE_PAGE_KEYBOARD)
                is_keyboard = true;

            print_field(f);
        }
    }

    for (auto le = output_reports.next; le != &output_reports; le = le->next) {
        auto& r = containing_record<usb_hid_report>(le);

        print("output report {}:\n", r.id);

        for (auto le2 = r.fields.next; le2 != &r.fields; le2 = le2->next) {
            auto& f = containing_record<usb_hid_field>(le2);

            print_field(f);
        }
    }

    this->packet_size = packet_size;

    auto pptr = new pipe(evdev_buffer_pages);
    if (!pptr) {
        print("new failed\n");
        return;
    }

    evdev_pipe = object_ref{*pptr};

    unsigned int num = hid_num.fetch_add(1);

    {
        char s[50];
        auto sp = span(s, sizeof(s));

        sp = format(sp, "usbhid{}", num);

        auto sv = string_view(s, (size_t)(sp.data() - s));

        // FIXME - put in /dev/input like on Linux?
        auto status = name_object(evdev_pipe.get(), sv);
        if (!success(status)) {
            print("name_object returned {}\n", status);
            return;
        }
    }

    if (is_keyboard) {
        this->kc = register_keyboard(static_cast<pipe&>(evdev_pipe.get()), [](void* ctx, keyboard_leds leds) {
            auto& kbd = *(usb_hid_device*)ctx;

            kbd.set_leds(leds);
        }, this);

        repeat_timer = create_timer(true);
        if (!repeat_timer) {
            print("create_timer failed\n");
            return;
        }

        acquire();
        if (!add_kernel_thread([](void* ctx) { ((usb_hid_device*)ctx)->keyboard_repeat_thread(); }, this)) {
            release();
            print("add_kernel_thread failed\n");
            return;
        }
    }
}

usb_hid_device::~usb_hid_device() {
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

    while (!is_list_empty(input_reports)) {
        auto& r = containing_record<usb_hid_report>(input_reports.next);

        remove_entry_list(r.le);

        while (!is_list_empty(r.fields)) {
            auto& f = containing_record<usb_hid_field>(r.fields.next);

            remove_entry_list(f.le);

            delete &f;
        }

        delete &r;
    }

    while (!is_list_empty(output_reports)) {
        auto& r = containing_record<usb_hid_report>(output_reports.next);

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

void usb_hid_device::set_leds(keyboard_leds leds) {
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

void usb_hid_device::post_evdev(enum evdev_code code, int32_t value) {
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

static uint32_t extract_num(span<const uint8_t> sp, unsigned int offset,
                            unsigned int size) {
    uint32_t ret = 0;

    for (unsigned int i = 0; i < size; i++) {
        unsigned int byte_index = (offset + i) / 8;
        unsigned int bit_index = (offset + i) % 8;

        if (byte_index < sp.size() && (sp[byte_index] >> bit_index) & 1)
            ret |= (uint32_t)1 << i;
    }

    return ret;
}

void usb_hid_device::input_usage(uint16_t usage_page, uint32_t usage, int32_t num) {
    if (usage_page == USAGE_PAGE_KEYBOARD) {
        if (usage > 255)
            return;

        if (num)
            keys[usage / 64] |= (uint64_t)1 << (usage % 64);

        return;
    }

    // ignore vendor-defined usages
    if (usage_page >= 0xff00)
        return;

    print("input_usage: usage page {:x}, usage {:x}, num {}\n",
          usage_page, usage, num);

    // FIXME
}

void usb_hid_device::handle_input_field(usb_hid_field& f, span<const uint8_t> sp) {
    if (f.is_variable) {
        unsigned int offset = f.bit_offset;

        for (unsigned int i = 0; i < f.count; i++) {
            int32_t signed_num;

            auto num = extract_num(sp, offset, f.size);

            if (f.logical_min < 0 && (num & ((uint32_t)1 << (f.size - 1))))
                signed_num = (int32_t)(num | (~(uint32_t)0 << f.size));
            else
                signed_num = (int32_t)num;

            uint32_t usage;

            if (f.num_usages > 0) {
                if (f.num_usages > i)
                    usage = f.usages[i];
                else
                    usage = f.usages[f.num_usages - 1];
            } else
                usage = f.usage_min + i;

            input_usage(f.usage_page, usage, signed_num);

            offset += f.size;
        }
    } else {
        unsigned int offset = f.bit_offset;

        if (!f.old) {
            f.old = malloc(sp.size());
            if (!f.old) {
                print("malloc failed\n");
                return;
            }

            memset(f.old, 0, sp.size());
        }

        auto old = span((const uint8_t*)f.old, sp.size());

        auto in_report = [&f](span<const uint8_t> sp, uint32_t usage) {
            unsigned int offset = f.bit_offset;

            for (unsigned int i = 0; i < f.count; i++) {
                if (extract_num(sp, offset, f.size) == usage)
                    return true;

                offset += f.size;
            }

            return false;
        };

        for (unsigned int i = 0; i < f.count; i++) {
            auto usage = extract_num(old, offset, f.size);

            if (usage != 0 && usage >= f.usage_min && usage <= f.usage_max) {
                if (!in_report(sp, usage))
                    input_usage(f.usage_page, usage, 0);
            }

            offset += f.size;
        }

        offset = f.bit_offset;

        for (unsigned int i = 0; i < f.count; i++) {
            auto usage = extract_num(sp, offset, f.size);

            if (usage != 0 && usage >= f.usage_min && usage <= f.usage_max) {
                if (!in_report(old, usage))
                    input_usage(f.usage_page, usage, 1);
            }

            offset += f.size;
        }

        memcpy(f.old, sp.data(), sp.size());
    }
}

void usb_hid_device::handle_input(usb_hid_report& r, span<const uint8_t> sp) {
    if (r.id != 0) {
        if (sp.empty() || sp[0] != r.id)
            return;

        sp = sp.subspan(1);
    }

    for (auto le = r.fields.next; le != &r.fields; le = le->next) {
        auto& f = containing_record<usb_hid_field>(le);

        if (f.usage_page == USAGE_PAGE_KEYBOARD) {
            auto clear_bit = [this](uint32_t usage) {
                keys[usage / 64] &= ~((uint64_t)1 << (usage % 64));
            };

            if (f.num_usages > 0) {
                for (unsigned int i = 0; i < f.num_usages; i++) {
                    if (f.usages[i] < 256)
                        clear_bit(f.usages[i]);
                }
            } else {
                for (unsigned int i = f.usage_min; i <= f.usage_max; i++) {
                    if (i < 256)
                        clear_bit(i);
                }
            }
        }
    }

    for (auto le = r.fields.next; le != &r.fields; le = le->next) {
        auto& f = containing_record<usb_hid_field>(le);

        handle_input_field(f, sp);
    }
}

void usb_hid_device::recv_thread() {
    auto buf = (uint8_t*)malloc(packet_size);
    if (!buf) {
        print("malloc failed\n");
        return;
    }

    array<uint64_t, 4> old_keys;

    old_keys[0] = old_keys[1] = old_keys[2] = old_keys[3] = 0;

    while (true) {
        auto recv_bytes = queue.normal_transfer(span(buf, packet_size));

        if (recv_bytes == 0) {
            if (!d.dead)
                print("xhci_queue::normal_transfer failed\n");
            break;
        }

        memset(buf + recv_bytes, 0, packet_size - recv_bytes);

        if (is_keyboard)
            keys = old_keys;

        for (auto le = input_reports.next; le != &input_reports; le = le->next) {
            auto& r = containing_record<usb_hid_report>(le);

            handle_input(r, span(buf, packet_size));
        }

        if (is_keyboard) {
            if (memcmp(keys.data(), old_keys.data(), keys.size() * sizeof(uint64_t))) {
                for (size_t i = 0; i < keys.size(); i++) {
                    auto diff = keys[i] ^ old_keys[i];

                    while (diff != 0) {
                        auto bit = (size_t)countr_zero(diff);
                        auto code = (i * 64) + bit;

                        if (keycodes[code] == evdev_code::KEY_RESERVED)
                            print("unrecognized keycode {:x}\n", code);
                        else {
                            if (keys[i] & (uint64_t)1 << bit)
                                post_evdev(keycodes[code], 1);
                            else
                                post_evdev(keycodes[code], 0);
                        }

                        diff &= ~((uint64_t)1 << bit);
                    }
                }

                old_keys = keys;
            }
        }
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

void usb_hid_device::keyboard_repeat_thread() {
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

static bool found_usb_hid_device(usb_device& d, const usb_device_config& config,
                                 const usb_interface& interface) {
    if (interface.desc.bAlternateSetting) // FIXME?
        return false;

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
            print("found_usb_hid_device: multiple potential endpoints found\n");
            return false;
        }

        ep_desc = &e;
    }

    if (!ep_desc) {
        print("found_usb_hid_device: no potential endpoint found\n");
        return false;
    }

    if (ep_desc->desc.wMaxPacketSize == 0) {
        print("found_usb_hid_device: wMaxPacketSize was 0\n");
        return false;
    }

    if (interface.desc.bInterfaceSubClass == 1) {
        // change to report protocol
        if (!set_protocol(d, 1, interface.desc.bInterfaceNumber)) {
            print("found_usb_hid_device: set_protocol failed\n");
            return false;
        }
    }

    auto queue = d.configure_endpoint(ep_desc->desc);
    if (!queue) {
        print("usb_device::configure_endpoint failed\n");
        return false;
    }

    auto dev = new usb_hid_device(d, config, *queue,
                                  ep_desc->desc.wMaxPacketSize, interface);
    if (!dev) {
        print("new failed\n");
        return false;
    }

    add_kernel_thread([](void* ctx) {
        auto& dev = *(usb_hid_device*)ctx;

        dev.recv_thread();
        dev.release();
    }, dev);

    return true;
}
