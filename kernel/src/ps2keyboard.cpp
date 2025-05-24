module;

#include "kernel.h"
#include "spinlock.h"
import evdev;
#include "qwerty.h"
#include "handle.h"
import pipe;
#include "acpica.h"
#include "tick_rate.h"

export module ps2keyboard;

import print;
import inout;
import mem;
import interrupts;
import event;
import acpi;
import keyboard;
import timer;

using namespace std;

static const unsigned int keyboard_evdev_buffer_pages = 1;

static optional<reference_wrapper<event>> keyboard_event_obj, keyboard_ack_event_obj;
static object_ref keyboard_evdev_obj;

static const unsigned int MODIFIER_EXTD = 4;

static spinlock keyboard_lock;
static uint8_t* keyboard_buffer;
static unsigned int keyboard_buffer_write_pos = 0;
static unsigned int keyboard_buffer_read_pos = 0;
static unsigned int modifier = 0;

static uint64_t keydown1 = 0, keydown2 = 0;
static uint64_t keydown_extended1 = 0, keydown_extended2 = 0;
static enum evdev_code repeat_key = evdev_code::KEY_RESERVED;
static object_ref repeat_timer;

#define ONE_MS_TICKS (SYSTEM_TIME_RESOLUTION / 1000) // FIXME

static const uint64_t REPEAT_DELAY_MS = 660;
static const uint64_t REPEAT_RATE_HZ = 25;

static bool keyboard_initialized = false;
static uint16_t keyboard_port1, keyboard_port2;

static bool keyboard_isr_func(void*);

static void found_ps2_keyboard(const acpi_device& d);

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
};

static void init_acpi_hids() {
    for (unsigned int i = 0; i < sizeof(mod_pnp_hids) / sizeof(*mod_pnp_hids); i++) {
        insert_tail_list(pnp_hids, mod_pnp_hids[i].le);
    }
}

static module_init module_init_keyboard(init_acpi_hids);

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

static void post_evdev(enum evdev_code code, unsigned int value) {
    struct input_event ev;
    auto& p = static_cast<pipe&>(keyboard_evdev_obj.get());

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

static void keyboard_event_recv() {
    do {
        wait_for_object(keyboard_event_obj->get(), false);

        lock_guard lock(keyboard_lock);

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
                    evdev_codes_modified[sc & 0x7f] != evdev_code::KEY_RESERVED) {
                    post_evdev(evdev_codes_modified[sc & 0x7f], !(sc & 0x80) ? 1 : 0);
                }

                modifier &= ~MODIFIER_EXTD;
            } else {
                if ((sc & 0x7f) < sizeof(evdev_codes) / sizeof(*evdev_codes) &&
                    evdev_codes[sc & 0x7f] != evdev_code::KEY_RESERVED) {
                    post_evdev(evdev_codes[sc & 0x7f], !(sc & 0x80) ? 1 : 0);
                }
            }
        }
    } while (true);
}

static void keyboard_set_leds(void*, keyboard_leds leds) {
    while (inb(keyboard_port2) & 2) { }

    outb(keyboard_port1, 0xed);

    wait_for_object(keyboard_ack_event_obj->get(), false);

    while (inb(keyboard_port2) & 2) { }

    uint8_t state = 0;

    if (leds.scroll_lock)
        state |= 1 << 0;

    if (leds.num_lock)
        state |= 1 << 1;

    if (leds.caps_lock)
        state |= 1 << 2;

    outb(keyboard_port1, state);

    wait_for_object(keyboard_ack_event_obj->get(), false);
}

static bool add_keyboard_evdev_pipe() {
    auto pptr = new pipe(keyboard_evdev_buffer_pages);
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    keyboard_evdev_obj = object_ref{*pptr};

    // FIXME - put in /dev/input like on Linux
    if (!name_object(keyboard_evdev_obj.get(), "ps2keyboard")) {
        print("name_object failed\n");
        return false;
    }

    register_keyboard(static_cast<pipe&>(keyboard_evdev_obj.get()),
                      keyboard_set_leds, nullptr);

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

    auto keyboard_event = create_event(true);
    if (!keyboard_event) {
        print("create_event failed\n");
        return;
    }

    auto keyboard_ack_event = create_event(true);
    if (!keyboard_ack_event) {
        print("create_event failed\n");
        return;
    }

    keyboard_event_obj = static_cast<event&>(keyboard_event.get());
    keyboard_event_obj->get().acquire();

    keyboard_ack_event_obj = static_cast<event&>(keyboard_ack_event.get());
    keyboard_ack_event_obj->get().acquire();

    if (!add_kernel_thread(keyboard_event_recv)) {
        print("add_kernel_thread failed\n");
        return;
    }

    if (!add_keyboard_evdev_pipe()) {
        print("add_keyboard_evdev_pipe failed\n");
        return;
    }

    __asm __volatile("cli");

    while (inb(port2) & 1) {
        inb(port1);
    }

    auto local_apic_id = *(volatile uint32_t*)&local_apic[0x20];

    __asm __volatile("sti");

    repeat_timer = create_timer(true);
    if (!repeat_timer) {
        print("create_timer failed\n");
        return;
    }

    if (!add_kernel_thread(keyboard_repeat_thread)) {
        print("add_kernel_thread failed\n");
        return;
    }

    if (!add_irq_handler(irq, local_apic_id, keyboard_isr_func, nullptr)) {
        print("add_irq_handler failed\n");
        return;
    }

    keyboard_port1 = port1;
    keyboard_port2 = port2;
    keyboard_initialized = true;
}

static bool keyboard_isr_func(void*) {
    if (!(inb(keyboard_port2) & 0x1)) // not for us
        return false;

    auto code = inb(keyboard_port1);

    {
        lock_guard lock(keyboard_lock);

        if (code == 0xfa) {
            set_event(keyboard_ack_event_obj->get());
            return true;
        }

        keyboard_buffer[keyboard_buffer_write_pos] = code;

        keyboard_buffer_write_pos++;
        keyboard_buffer_write_pos &= 0xfff;
    }

    set_event(keyboard_event_obj->get());

    return true;
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
}
