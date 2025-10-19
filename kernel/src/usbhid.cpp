module;

#include <string_view>
#include <span>
#include <mutex>
#include "tick_rate.h"
#include "handle.h"
#include <ioctl.h>

export module usbhid;

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

using namespace std;

#define ONE_MS_TICKS (SYSTEM_TIME_RESOLUTION / 1000) // FIXME

static const int64_t REPEAT_DELAY_MS = 660;
static const int64_t REPEAT_RATE_HZ = 25;

static const size_t evdev_buffer_pages = 1;

static atomic<unsigned int> keyboard_num = 0;

class usb_keyboard {
public:
    usb_keyboard(usb_device& d, xhci_queue& queue, uint16_t packet_size,
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

    usb_device& d;
    xhci_queue& queue;
    object_ref evdev_pipe;
    uint16_t packet_size;
    const usb_interface& interface;
    keyboard_context* kc = nullptr;
    enum evdev_code repeat_key = evdev_code::KEY_RESERVED;
    object_ref repeat_timer;
    atomic<unsigned int> refcount = 1;
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

usb_keyboard::usb_keyboard(usb_device& d, xhci_queue& queue, uint16_t packet_size,
                           const usb_interface& interface) : d(d), queue(queue), interface(interface) {
    d.acquire();

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

    d.release();
}

void usb_keyboard::set_leds(keyboard_leds leds) {
    auto buf = (uint8_t*)malloc(1);
    if (!buf) {
        print("malloc failed\n");
        return;
    }

    buf[0] = 0;

    if (leds.num_lock)
        buf[0] |= 1 << 0;

    if (leds.caps_lock)
        buf[0] |= 1 << 1;

    if (leds.scroll_lock)
        buf[0] |= 1 << 2;

    if (!usb_hid_set_report(d, span(buf, 1), usb_hid_report_type::OUTPUT, 0,
                            interface.desc.bInterfaceNumber)) {
        print("usb_hid_set_report failed\n");
        free(buf);
        return;
    }

    free(buf);
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

static bool found_usb_keyboard(usb_device& d, const usb_interface& interface) {
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

    auto kbd = new usb_keyboard(d, *queue, ep_desc->desc.wMaxPacketSize,
                                interface);
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

static bool found_usb_hid_device(usb_device& d, const usb_device_config&,
                                 const usb_interface& interface) {
    // FIXME - should be parsing HID descriptors

    if (interface.desc.bAlternateSetting)
        return false;

    // FIXME - reject anything for now that doesn't have a keyboard boot interface
    if (interface.desc.bInterfaceSubClass != 1 || interface.desc.bInterfaceProtocol != 1)
        return false;

    return found_usb_keyboard(d, interface);
}
