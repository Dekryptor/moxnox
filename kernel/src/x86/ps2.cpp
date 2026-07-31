#include <span>
#include <optional>
#include <mutex>

import nt;
import print;
import inout;
import mem;
import interrupts;
import event;
import acpi;
import evdev_device;
import timer;
import module_init;
import thread;
import disable_interrupts;
import scheduler;
import list;
import spinlock;
import handle;
import object_ref;
import config;
import acpica;
import moxnox_ioctls;
import pipe;
import evdev;
import rwsem;
import vec;

using namespace std;

#include "../qwerty.h"

static const unsigned int keyboard_evdev_buffer_pages = 1;

// 8042 controller commands (sent to port2)
static const uint8_t I8042_CMD_READ_CONFIG = 0x20;
static const uint8_t I8042_CMD_WRITE_CONFIG = 0x60;
static const uint8_t I8042_CMD_DISABLE_MOUSE = 0xa7;
static const uint8_t I8042_CMD_ENABLE_MOUSE = 0xa8;
static const uint8_t I8042_CMD_TEST_MOUSE = 0xa9;
static const uint8_t I8042_CMD_SELF_TEST = 0xaa;
static const uint8_t I8042_CMD_TEST_KEYBOARD = 0xab;
static const uint8_t I8042_CMD_MOUSE_PREFIX = 0xd4;

// 8042 status register bits (read from port2)
static const uint8_t I8042_STR_OBF = 0x01; // output buffer full
static const uint8_t I8042_STR_IBF = 0x02; // input buffer full
static const uint8_t I8042_STR_AUXDATA = 0x20; // data from mouse

// 8042 config byte bits
static const uint8_t I8042_CFG_KBD_IRQ = 0x01;
static const uint8_t I8042_CFG_AUX_IRQ = 0x02;
static const uint8_t I8042_CFG_KBD_CLOCK_DIS = 0x10;
static const uint8_t I8042_CFG_AUX_CLOCK_DIS = 0x20;

// Keyboard commands (sent to port1)
static const uint8_t KBD_CMD_SET_LEDS = 0xed;
static const uint8_t KBD_CMD_SET_SCANCODE = 0xf0;
static const uint8_t KBD_CMD_GET_ID = 0xf2;
static const uint8_t KBD_CMD_SET_RATE = 0xf3;
static const uint8_t KBD_CMD_ENABLE = 0xf4;
static const uint8_t KBD_CMD_RESET = 0xff;

// Mouse commands (sent via 0xd4 prefix)
static const uint8_t MOUSE_CMD_SET_RATE = 0xf3;
static const uint8_t MOUSE_CMD_GET_ID = 0xf2;
static const uint8_t MOUSE_CMD_ENABLE = 0xf4;
static const uint8_t MOUSE_CMD_RESET = 0xff;

// Response bytes
static const uint8_t PS2_ACK = 0xfa;
static const uint8_t PS2_RESEND = 0xfe;
static const uint8_t PS2_SELF_TEST_PASS = 0xaa;
static const uint8_t PS2_SELF_TEST_FAIL = 0xfc;

// Mouse IDs
static const uint8_t MOUSE_ID_STANDARD = 0x00;
static const uint8_t MOUSE_ID_IMPS2 = 0x03;
static const uint8_t MOUSE_ID_IMEX = 0x04;

// Mouse packet bits (byte 0)
static const uint8_t MOUSE_BTN_LEFT = 0x01;
static const uint8_t MOUSE_BTN_RIGHT = 0x02;
static const uint8_t MOUSE_BTN_MIDDLE = 0x04;
static const uint8_t MOUSE_SYNC_BIT = 0x08;
static const uint8_t MOUSE_X_SIGN = 0x10;
static const uint8_t MOUSE_Y_SIGN = 0x20;
static const uint8_t MOUSE_X_OVERFLOW = 0x40;
static const uint8_t MOUSE_Y_OVERFLOW = 0x80;

static const uint8_t MOUSE_IMEX_BTN_SIDE = 0x10;
static const uint8_t MOUSE_IMEX_BTN_EXTRA = 0x20;

static object_ref keyboard_evdev_obj, keyboard_event_obj, keyboard_ack_event_obj;

static const unsigned int MODIFIER_EXTD = 4;

static spinlock ps2_lock;
static uint8_t* keyboard_buffer;
static unsigned int keyboard_buffer_write_pos = 0;
static unsigned int keyboard_buffer_read_pos = 0;
static unsigned int modifier = 0;
static bool caps_lock = false, num_lock = false, scroll_lock = false;
static rwsem leds_rwsem;

static uint64_t keydown1 = 0, keydown2 = 0;
static uint64_t keydown_extended1 = 0, keydown_extended2 = 0;
static enum evdev_key_code repeat_key = evdev_key_code::KEY_RESERVED;
static object_ref repeat_timer;

static const int64_t REPEAT_DELAY_MS = 660;
static const int64_t REPEAT_RATE_HZ = 25;

static bool keyboard_initialized = false;
static uint16_t keyboard_port1, keyboard_port2;

static bool mouse_initialized = false, mouse_init_pending = false;
static uint32_t mouse_irq;
static object_ref mouse_ack_event, mouse_evdev_obj;
static uint8_t mouse_packet[4];
static size_t mouse_packet_len = 0;
static bool mouse_left = false, mouse_right = false, mouse_middle = false;
static bool mouse_side = false, mouse_extra = false;
static bool mouse_expecting_ack = false;

enum class mouse_type {
    normal,
    imps2, // Intellimouse PS/2
    imex // Intellimouse Explorer
};

static mouse_type mouse_type = mouse_type::normal;

static bool ps2_isr_func(void*);

class ps2_keyboard : public evdev_device {
public:
    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override;
    nt_status write_vec(span<const span<const uint8_t>> v,
                        uint64_t& written_bytes) override;
};

class ps2_mouse : public evdev_device {
public:
    nt_status write(span<const uint8_t>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status write_vec(span<const span<const uint8_t>>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }
};

static void found_ps2_keyboard(const acpi_device& d);
static void found_ps2_mouse(const acpi_device& d);

static acpi_recognizer mod_pnp_hids[] = {
    { {}, "PNP0300", found_ps2_keyboard, nullptr },
    { {}, "PNP0301", found_ps2_keyboard, nullptr },
    { {}, "PNP0302", found_ps2_keyboard, nullptr },
    { {}, "PNP0303", found_ps2_keyboard, nullptr },
    { {}, "PNP0304", found_ps2_keyboard, nullptr },
    { {}, "PNP0305", found_ps2_keyboard, nullptr },
    { {}, "PNP0306", found_ps2_keyboard, nullptr },
    { {}, "PNP0309", found_ps2_keyboard, nullptr },
    { {}, "PNP030A", found_ps2_keyboard, nullptr },
    { {}, "PNP030B", found_ps2_keyboard, nullptr },
    { {}, "PNP0320", found_ps2_keyboard, nullptr },
    { {}, "PNP0343", found_ps2_keyboard, nullptr },
    { {}, "PNP0344", found_ps2_keyboard, nullptr },
    { {}, "PNP0345", found_ps2_keyboard, nullptr },
    { {}, "CPQA0D7", found_ps2_keyboard, nullptr },
    { {}, "PNP0F13", found_ps2_mouse, nullptr },
};

static void init_acpi_hids() {
    for (unsigned int i = 0; i < sizeof(mod_pnp_hids) / sizeof(*mod_pnp_hids); i++) {
        insert_tail_list(pnp_hids, mod_pnp_hids[i].le);
    }
}

static module_init module_init_keyboard(init_acpi_hids);

static bool should_key_repeat(enum evdev_key_code code) {
    switch (code) {
        case evdev_key_code::KEY_RESERVED:
        case evdev_key_code::KEY_LEFTALT:
        case evdev_key_code::KEY_RIGHTALT:
        case evdev_key_code::KEY_LEFTCTRL:
        case evdev_key_code::KEY_RIGHTCTRL:
        case evdev_key_code::KEY_LEFTSHIFT:
        case evdev_key_code::KEY_RIGHTSHIFT:
        case evdev_key_code::KEY_LEFTMETA:
        case evdev_key_code::KEY_RIGHTMETA:
        case evdev_key_code::KEY_CAPSLOCK:
        case evdev_key_code::KEY_NUMLOCK:
        case evdev_key_code::KEY_SCROLLLOCK:
            return false;

        default:
            return true;
    }
}

static void post_key_evdev(enum evdev_key_code code, int32_t value) {
    input_event ev;
    auto& p = static_cast<ps2_keyboard&>(keyboard_evdev_obj.get());

    if (value != 2 && should_key_repeat(code)) {
        auto& t = static_cast<timer&>(repeat_timer.get());

        if (value == 0) { // up
            if (repeat_key == code) {
                if (auto status = t.cancel(nullptr); !success(status))
                    print("timer::cancel returned {}\n", status);

                repeat_key = evdev_key_code::KEY_RESERVED;
            }
        } else { // down
            if (repeat_key == evdev_key_code::KEY_RESERVED) {
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

    // FIXME - Linux also has EV_MSC events

    ev.type = evdev_type::EV_KEY;
    ev.code = (evdev_code)code;
    ev.value = value;

    p.post_event(ev, true);
}

static void keyboard_event_recv() {
    do {
        wait_for_object(keyboard_event_obj.get(), false);

        lock_guard lock(ps2_lock);

        while (keyboard_buffer_read_pos != keyboard_buffer_write_pos) {
            auto sc = keyboard_buffer[keyboard_buffer_read_pos];

            keyboard_buffer_read_pos++;
            keyboard_buffer_read_pos &= 0xfff;

            // FIXME - print screen (e0 2a e0 37)
            // FIXME - pause (e1 1d 45)
            // FIXME - Fn key (e0 63) (what is this in evdev?)

            if (sc != CODE_EXTD) {
                uint64_t keydown_mask;
                uint64_t* keydown_ptr;

                keydown_mask = (uint64_t)1 << (sc & 0x3f);

                if (modifier & MODIFIER_EXTD) {
                    if (sc & 0x40)
                        keydown_ptr = &keydown_extended2;
                    else
                        keydown_ptr = &keydown_extended1;
                } else {
                    if (sc & 0x40)
                        keydown_ptr = &keydown2;
                    else
                        keydown_ptr = &keydown1;
                }

                if (sc & 0x80)
                    *keydown_ptr &= ~keydown_mask;
                else {
                    if (*keydown_ptr & keydown_mask)
                        continue; // ignore repeats

                    *keydown_ptr |= keydown_mask;
                }
            }

            if (sc == CODE_EXTD)
                modifier |= MODIFIER_EXTD;
            else if (modifier & MODIFIER_EXTD) {
                if ((sc & 0x7f) < sizeof(evdev_codes_modified) / sizeof(*evdev_codes_modified) &&
                    evdev_codes_modified[sc & 0x7f] != evdev_key_code::KEY_RESERVED) {
                    post_key_evdev(evdev_codes_modified[sc & 0x7f], !(sc & 0x80) ? 1 : 0);
                }

                modifier &= ~MODIFIER_EXTD;
            } else {
                if ((sc & 0x7f) < sizeof(evdev_codes) / sizeof(*evdev_codes) &&
                    evdev_codes[sc & 0x7f] != evdev_key_code::KEY_RESERVED) {
                    post_key_evdev(evdev_codes[sc & 0x7f], !(sc & 0x80) ? 1 : 0);
                }
            }
        }
    } while (true);
}

nt_status ps2_keyboard::write_vec(span<const span<const uint8_t>> v,
                                  uint64_t& written_bytes) {
    if (get_vec_size(v) < sizeof(input_event))
        return nt_status::INVALID_PARAMETER;

    input_event ev;
    span<const uint8_t> top = v.front();

    memcpy_vec((uint8_t*)&ev, v, top, sizeof(input_event));

    if (ev.type != evdev_type::EV_LED)
        return nt_status::NOT_SUPPORTED;

    switch ((evdev_led_code)ev.code) {
        case evdev_led_code::LED_CAPSL:
            caps_lock = ev.value ? 1 : 0;
        break;

        case evdev_led_code::LED_NUML:
            num_lock = ev.value ? 1 : 0;
        break;

        case evdev_led_code::LED_SCROLLL:
            scroll_lock = ev.value ? 1 : 0;
        break;

        default:
            return nt_status::NOT_SUPPORTED;
    }

    {
        lock_guard lock(leds_rwsem);

        {
            lock_guard lock2(ps2_lock);

            while (inb(keyboard_port2) & I8042_STR_IBF) { }

            outb(keyboard_port1, KBD_CMD_SET_LEDS);
        }

        wait_for_object(keyboard_ack_event_obj.get(), false);

        {
            lock_guard lock2(ps2_lock);

            while (inb(keyboard_port2) & I8042_STR_IBF) { }

            uint8_t state = 0;

            if (scroll_lock)
                state |= 1 << 0;

            if (num_lock)
                state |= 1 << 1;

            if (caps_lock)
                state |= 1 << 2;

            outb(keyboard_port1, state);
        }

        wait_for_object(keyboard_ack_event_obj.get(), false);
    }

    written_bytes = sizeof(input_event);

    return nt_status::SUCCESS;
}

nt_status ps2_keyboard::write(span<const uint8_t> s, uint64_t& written_bytes) {
    return write_vec(span(&s, 1), written_bytes);
}

static bool add_keyboard_evdev_pipe() {
    auto pptr = new ps2_keyboard;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    keyboard_evdev_obj = object_ref{*pptr};

    // FIXME - put in /dev/input like on Linux
    auto status = name_object(keyboard_evdev_obj.get(), "ps2keyboard");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    register_evdev_device(*pptr);

    return true;
}

static void keyboard_repeat_thread() {
    while (true) {
        auto status = wait_for_object(repeat_timer.get(), false);
        if (!success(status)) {
            print("wait_for_object returned {}\n", status);
            break;
        }

        auto code = repeat_key;

        if (code == evdev_key_code::KEY_RESERVED)
            continue;

        post_key_evdev(code, 2);

        auto& t = static_cast<timer&>(repeat_timer.get());

        if (auto status = t.set(-(SYSTEM_TIME_RESOLUTION / REPEAT_RATE_HZ), nullptr,
                                nullptr, false, 0, nullptr); !success(status)) {
            print("timer::set returned {}\n", status);
            break;
        }
    }
}

static void init_keyboard(uint16_t port1, uint16_t port2, uint32_t irq) {
    if (keyboard_initialized) // can only be one PS/2 keyboard(?)
        return;

    auto pa = allocate_page();
    if (!pa) {
        print("allocate_page failed\n");
        return;
    }

    keyboard_buffer = (uint8_t*)phys_address(pa);

    keyboard_event_obj = create_event(true);
    if (!keyboard_event_obj) {
        print("create_event failed\n");
        return;
    }

    keyboard_ack_event_obj = create_event(true);
    if (!keyboard_ack_event_obj) {
        print("create_event failed\n");
        return;
    }

    if (!add_kernel_thread(keyboard_event_recv)) {
        print("add_kernel_thread failed\n");
        return;
    }

    if (!add_keyboard_evdev_pipe()) {
        print("add_keyboard_evdev_pipe failed\n");
        return;
    }

    {
        lock_guard lock(ps2_lock);

        while (inb(port2) & I8042_STR_OBF) {
            inb(port1);
        }
    }

    repeat_timer = create_timer(true);
    if (!repeat_timer) {
        print("create_timer failed\n");
        return;
    }

    if (!add_kernel_thread(keyboard_repeat_thread)) {
        print("add_kernel_thread failed\n");
        return;
    }

    if (!add_irq_handler(irq, false, ps2_isr_func, nullptr)) {
        print("add_irq_handler failed\n");
        return;
    }

    keyboard_port1 = port1;
    keyboard_port2 = port2;
    keyboard_initialized = true;
}

static void process_mouse_packet() {
    if (mouse_packet[0] & (MOUSE_X_OVERFLOW | MOUSE_Y_OVERFLOW))
        return;

    int16_t rel_x = mouse_packet[1];
    if (rel_x != 0 && mouse_packet[0] & MOUSE_X_SIGN)
        rel_x |= (int16_t)0xff00;

    int16_t rel_y = mouse_packet[2];
    if (rel_y != 0 && mouse_packet[0] & MOUSE_Y_SIGN)
        rel_y |= (int16_t)0xff00;

    rel_y = (int16_t)-rel_y; // opposite direction to USB

    auto& m = static_cast<ps2_mouse&>(mouse_evdev_obj.get());
    bool posted = false;
    input_event ev;

    if (rel_x != 0 || rel_y != 0) {
        if (rel_x != 0) {
            ev.type = evdev_type::EV_REL;
            ev.code = (evdev_code)evdev_rel_code::REL_X;
            ev.value = rel_x;

            m.post_event(ev, false);
        }

        if (rel_y != 0) {
            ev.type = evdev_type::EV_REL;
            ev.code = (evdev_code)evdev_rel_code::REL_Y;
            ev.value = rel_y;

            m.post_event(ev, false);
        }

        posted = true;
    }

    if (mouse_left && !(mouse_packet[0] & MOUSE_BTN_LEFT)) {
        mouse_left = false;

        ev.type = evdev_type::EV_KEY;
        ev.code = (evdev_code)evdev_key_code::BTN_LEFT;
        ev.value = 0;

        m.post_event(ev, false);
        posted = true;
    } else if (!mouse_left && mouse_packet[0] & MOUSE_BTN_LEFT) {
        mouse_left = true;

        ev.type = evdev_type::EV_KEY;
        ev.code = (evdev_code)evdev_key_code::BTN_LEFT;
        ev.value = 1;

        m.post_event(ev, false);
        posted = true;
    }

    if (mouse_right && !(mouse_packet[0] & MOUSE_BTN_RIGHT)) {
        mouse_right = false;

        ev.type = evdev_type::EV_KEY;
        ev.code = (evdev_code)evdev_key_code::BTN_RIGHT;
        ev.value = 0;

        m.post_event(ev, false);
        posted = true;
    } else if (!mouse_right && mouse_packet[0] & MOUSE_BTN_RIGHT) {
        mouse_right = true;

        ev.type = evdev_type::EV_KEY;
        ev.code = (evdev_code)evdev_key_code::BTN_RIGHT;
        ev.value = 1;

        m.post_event(ev, false);
        posted = true;
    }

    if (mouse_middle && !(mouse_packet[0] & MOUSE_BTN_MIDDLE)) {
        mouse_middle = false;

        ev.type = evdev_type::EV_KEY;
        ev.code = (evdev_code)evdev_key_code::BTN_MIDDLE;
        ev.value = 0;

        m.post_event(ev, false);
        posted = true;
    } else if (!mouse_middle && mouse_packet[0] & MOUSE_BTN_MIDDLE) {
        mouse_middle = true;

        ev.type = evdev_type::EV_KEY;
        ev.code = (evdev_code)evdev_key_code::BTN_MIDDLE;
        ev.value = 1;

        m.post_event(ev, false);
        posted = true;
    }

    switch (mouse_type) {
        case mouse_type::normal:
            break;

        case mouse_type::imps2: {
            auto wheel = (int8_t)mouse_packet[3];

            if (wheel != 0) {
                ev.type = evdev_type::EV_REL;
                ev.code = (evdev_code)evdev_rel_code::REL_WHEEL;
                ev.value = wheel;

                m.post_event(ev, false);
                posted = true;
            }

            break;
        }

        case mouse_type::imex: {
            auto wheel = (int8_t)(mouse_packet[3] & 0xf);

            if (wheel & 0x8)
                wheel |= (int8_t)0xf0;

            if (wheel != 0) {
                ev.type = evdev_type::EV_REL;
                ev.code = (evdev_code)evdev_rel_code::REL_WHEEL;
                ev.value = wheel;

                m.post_event(ev, false);
                posted = true;
            }

            if (mouse_side && !(mouse_packet[3] & MOUSE_IMEX_BTN_SIDE)) {
                mouse_side = false;

                ev.type = evdev_type::EV_KEY;
                ev.code = (evdev_code)evdev_key_code::BTN_SIDE;
                ev.value = 0;

                m.post_event(ev, false);
                posted = true;
            } else if (!mouse_side && mouse_packet[3] & MOUSE_IMEX_BTN_SIDE) {
                mouse_side = true;

                ev.type = evdev_type::EV_KEY;
                ev.code = (evdev_code)evdev_key_code::BTN_SIDE;
                ev.value = 1;

                m.post_event(ev, false);
                posted = true;
            }

            if (mouse_extra && !(mouse_packet[3] & MOUSE_IMEX_BTN_EXTRA)) {
                mouse_extra = false;

                ev.type = evdev_type::EV_KEY;
                ev.code = (evdev_code)evdev_key_code::BTN_EXTRA;
                ev.value = 0;

                m.post_event(ev, false);
                posted = true;
            } else if (!mouse_extra && mouse_packet[3] & MOUSE_IMEX_BTN_EXTRA) {
                mouse_extra = true;

                ev.type = evdev_type::EV_KEY;
                ev.code = (evdev_code)evdev_key_code::BTN_EXTRA;
                ev.value = 1;

                m.post_event(ev, false);
                posted = true;
            }

            break;
        }
    }

    if (posted) {
        ev.type = evdev_type::EV_SYN;
        ev.code = (evdev_code)evdev_syn_code::SYN_REPORT;
        ev.value = 0;

        m.post_event(ev, false);
    }
}

static void mouse_received_byte(uint8_t code) {
    if (mouse_packet_len == 0 && !(code & MOUSE_SYNC_BIT))
        return; // out of sync

    mouse_packet[mouse_packet_len] = code;
    mouse_packet_len++;

    switch (mouse_type) {
        case mouse_type::normal:
            if (mouse_packet_len == 3) {
                process_mouse_packet();
                mouse_packet_len = 0;
            }
        break;

        case mouse_type::imps2:
        case mouse_type::imex:
            if (mouse_packet_len == 4) {
                process_mouse_packet();
                mouse_packet_len = 0;
            }
        break;
    }
}

static bool ps2_isr_func(void*) {
    uint8_t status, code;

    {
        lock_guard lock(ps2_lock);

        status = inb(keyboard_port2);

        if (!(status & I8042_STR_OBF)) // not for us
            return false;

        code = inb(keyboard_port1);
    }

    if (status & I8042_STR_AUXDATA) {
        if (mouse_expecting_ack && code == PS2_ACK) {
            set_event(static_cast<event&>(mouse_ack_event.get()));
            mouse_expecting_ack = false;
            return true;
        }

        mouse_received_byte(code);
    } else {
        if (code == PS2_ACK) {
            set_event(static_cast<event&>(keyboard_ack_event_obj.get()));
            return true;
        }

        {
            lock_guard lock(ps2_lock);

            keyboard_buffer[keyboard_buffer_write_pos] = code;

            keyboard_buffer_write_pos++;
            keyboard_buffer_write_pos &= 0xfff;

            set_event(static_cast<event&>(keyboard_event_obj.get()));
        }
    }

    return true;
}

static bool wait_for_output(uint16_t port2, optional<unsigned int> timeout = nullopt) {
    if (timeout.has_value()) {
        for (unsigned int i = 0; i < *timeout * 20; i++) {
            if (inb(port2) & I8042_STR_OBF)
                return true;

            udelay(50);
        }

        return false;
    } else {
        while (!(inb(port2) & I8042_STR_OBF)) { }

        return true;
    }
}

static bool set_mouse_sample_rate(uint16_t port1, uint16_t port2, uint8_t rate) {
    lock_guard lock(ps2_lock);

    while (inb(port2) & I8042_STR_IBF) { }
    outb(port2, I8042_CMD_MOUSE_PREFIX);
    while (inb(port2) & I8042_STR_IBF) { }
    outb(port1, MOUSE_CMD_SET_RATE);

    wait_for_output(port2);

    if (inb(port1) != PS2_ACK)
        return false;

    while (inb(port2) & I8042_STR_IBF) { }
    outb(port2, I8042_CMD_MOUSE_PREFIX);
    while (inb(port2) & I8042_STR_IBF) { }
    outb(port1, rate);

    wait_for_output(port2);

    if (inb(port1) != PS2_ACK)
        return false;

    return true;
}

static uint8_t get_mouse_id(uint16_t port1, uint16_t port2) {
    lock_guard lock(ps2_lock);

    while (inb(port2) & I8042_STR_IBF) { }
    outb(port2, I8042_CMD_MOUSE_PREFIX);
    while (inb(port2) & I8042_STR_IBF) { }
    outb(port1, MOUSE_CMD_GET_ID);

    wait_for_output(port2);

    if (inb(port1) != PS2_ACK)
        return 0;

    wait_for_output(port2);

    return inb(port1);
}

static void init_mouse(uint16_t port1, uint16_t port2, uint32_t irq) {
    if (mouse_initialized) // can only be one PS/2 mouse(?)
        return;

    mouse_ack_event = create_event(true);
    if (!mouse_ack_event) {
        print("create_event failed\n");
        return;
    }

    uint8_t id;

    {
        lock_guard lock(ps2_lock);

        // enable auxiliary device
        while (inb(port2) & I8042_STR_IBF) { }
        outb(port2, I8042_CMD_ENABLE_MOUSE);

        // read current config
        while (inb(port2) & I8042_STR_IBF) { }
        outb(port2, I8042_CMD_READ_CONFIG);
        wait_for_output(port2);
        auto config = inb(port1);

        config |= I8042_CFG_AUX_IRQ;
        config &= ~I8042_CFG_AUX_CLOCK_DIS;

        // write config back
        while (inb(port2) & I8042_STR_IBF) { }
        outb(port2, I8042_CMD_WRITE_CONFIG);
        while (inb(port2) & I8042_STR_IBF) { }
        outb(port1, config);

        while (inb(port2) & I8042_STR_IBF) { }
        outb(port2, I8042_CMD_MOUSE_PREFIX);
        while (inb(port2) & I8042_STR_IBF) { }
        outb(port1, MOUSE_CMD_RESET);

        if (!wait_for_output(port2, 100))
            return;

        if (inb(port1) != PS2_ACK)
            return; // no mouse

        if (!wait_for_output(port2, 100))
            return;

        if (inb(port1) != PS2_SELF_TEST_PASS)
            return; // hardware fault

        if (!wait_for_output(port2, 100))
            return;

        id = inb(port1);
    }

    if (id != 0x00)
        print("init_mouse: hardware reported ID of 0x{:02x}, expected 0x00\n", id);
    else {
        set_mouse_sample_rate(port1, port2, 200);
        set_mouse_sample_rate(port1, port2, 100);
        set_mouse_sample_rate(port1, port2, 80);

        id = get_mouse_id(port1, port2);

        if (id == 0x03) {
            mouse_type = mouse_type::imps2;

            set_mouse_sample_rate(port1, port2, 200);
            set_mouse_sample_rate(port1, port2, 200);
            set_mouse_sample_rate(port1, port2, 80);

            id = get_mouse_id(port1, port2);

            if (id == 0x04)
                mouse_type = mouse_type::imex;
        }
    }

    if (!add_irq_handler(irq, false, ps2_isr_func, nullptr)) {
        print("add_irq_handler failed\n");
        return;
    }

    {
        lock_guard lock(ps2_lock);

        mouse_expecting_ack = true;
        reset_event(static_cast<event&>(mouse_ack_event.get()));

        while (inb(port2) & I8042_STR_OBF) {
            inb(port1);
        }

        while (inb(port2) & I8042_STR_IBF) { }
        outb(port2, I8042_CMD_MOUSE_PREFIX);
        while (inb(port2) & I8042_STR_IBF) { }
        outb(port1, MOUSE_CMD_ENABLE);
    }

    wait_for_object(mouse_ack_event.get(), false);

    auto pptr = new ps2_mouse;
    if (!pptr) {
        print("new failed\n");
        return;
    }

    mouse_evdev_obj = object_ref{*pptr};

    // FIXME - put in /dev/input like on Linux
    auto status = name_object(mouse_evdev_obj.get(), "ps2mouse");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return;
    }

    register_evdev_device(*pptr);

    mouse_initialized = true;
}

static void found_ps2_keyboard(const acpi_device& d) {
    optional<uint16_t> port1, port2;
    optional<uint32_t> irq;

    for (auto le = d.resources.next; le != &d.resources; le = le->next) {
        auto& r = containing_record<acpi_device_resource>(le);

        if (r.type == acpi_device_resource_type::io) {
            auto& r2 = static_cast<acpi_device_resource_io&>(r);

            if (r2.io.IoDecode == ACPI_DECODE_16) {
                if (port1.has_value() && !port2.has_value())
                    port2 = r2.io.Minimum;
                else if (!port1.has_value())
                    port1 = r2.io.Minimum;
            }
        } else if (r.type == acpi_device_resource_type::irq) {
            auto& r2 = static_cast<acpi_device_resource_irq&>(r);

            irq = r2.irq.Interrupt;
        } else if (r.type == acpi_device_resource_type::extended_irq) {
            auto& r2 = static_cast<acpi_device_resource_extended_irq&>(r);

            irq = r2.extended_irq.Interrupt;
        }
    }

    if (!port1.has_value() || !port2.has_value() || !irq.has_value())
        return;

    init_keyboard(*port1, *port2, *irq);

    if (mouse_init_pending)
        init_mouse(*port1, *port2, mouse_irq);
}

static void found_ps2_mouse(const acpi_device& d) {
    optional<uint32_t> irq;

    for (auto le = d.resources.next; le != &d.resources; le = le->next) {
        auto& r = containing_record<acpi_device_resource>(le);

        if (r.type == acpi_device_resource_type::irq) {
            auto& r2 = static_cast<acpi_device_resource_irq&>(r);

            irq = r2.irq.Interrupt;
        } else if (r.type == acpi_device_resource_type::extended_irq) {
            auto& r2 = static_cast<acpi_device_resource_extended_irq&>(r);

            irq = r2.extended_irq.Interrupt;
        }
    }

    if (!irq.has_value())
        return;

    if (keyboard_initialized)
        init_mouse(keyboard_port1, keyboard_port2, *irq);
    else {
        mouse_irq = *irq;
        mouse_init_pending = true;
    }
}
