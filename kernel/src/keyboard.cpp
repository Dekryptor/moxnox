#include "kernel.h"
#include "unique_handle.h"
#include "objects.h"
#include "spinlock.h"
#include "qwerty.h"

using namespace std;

static const uint8_t keyboard_interrupt = 0x21;

unique_handle keyboard_event;
optional<reference_wrapper<event>> keyboard_event_obj;

#define MODIFIER_LSHIFT     1
#define MODIFIER_RSHIFT     2
#define MODIFIER_EXTD       4
#define MODIFIER_LCTRL      8
#define MODIFIER_RCTRL      16

static spinlock keyboard_lock;
static uint8_t* keyboard_buffer;
static unsigned int keyboard_buffer_write_pos = 0;
static unsigned int keyboard_buffer_read_pos = 0;
static unsigned int modifier = 0;

static void keyboard_event_recv(void*) {
    do {
        wait_for_object(keyboard_event.get());

        lock_guard lock(keyboard_lock);

        while (keyboard_buffer_read_pos != keyboard_buffer_write_pos) {
            auto sc = keyboard_buffer[keyboard_buffer_read_pos];
            string_view sv;

            keyboard_buffer_read_pos++;
            keyboard_buffer_read_pos &= 0xfff;

            // FIXME - caps lock and num lock (and lights?)
            // FIXME - ctrl
            // FIXME - print screen (e0 2a e0 37)
            // FIXME - pause (e1 1d 45)

            if (sc == CODE_EXTD)
                modifier |= MODIFIER_EXTD;
            else if (modifier & MODIFIER_EXTD) {
                switch (sc) {
                    case CODE_MODIFIED_ENTER:
                        sv = "\n";
                    break;

                    case CODE_MODIFIED_RCTRL:
                        modifier |= MODIFIER_RCTRL;
                    break;

                    case CODE_MODIFIED_RCTRL | 0x80:
                        modifier &= ~MODIFIER_RCTRL;
                    break;

                    case CODE_MODIFIED_SLASH:
                        sv = "/";
                    break;

                    case CODE_MODIFIED_HOME:
                        sv = "\x1b[H";
                    break;

                    case CODE_MODIFIED_UP:
                        sv = "\x1b[A";
                    break;

                    case CODE_MODIFIED_PGUP:
                        sv = "\x1b[5~";
                    break;

                    case CODE_MODIFIED_LEFT:
                        sv = "\x1b[D";
                    break;

                    case CODE_MODIFIED_RIGHT:
                        sv = "\x1b[C";
                    break;

                    case CODE_MODIFIED_END:
                        sv = "\x1b[F";
                    break;

                    case CODE_MODIFIED_DOWN:
                        sv = "\x1b[B";
                    break;

                    case CODE_MODIFIED_PGDN:
                        sv = "\x1b[6~";
                    break;

                    case CODE_MODIFIED_INS:
                        sv = "\x1b[2~";
                    break;

                    case CODE_MODIFIED_DEL:
                        sv = "\x1b[3~";
                    break;

                    // FIXME - left win (5b), right win (5c), menu (5d)
                }

                modifier &= ~MODIFIER_EXTD;
            } else {
                switch (sc) {
                    case CODE_LSHIFT:
                        modifier |= MODIFIER_LSHIFT;
                    break;

                    case CODE_LSHIFT | 0x80:
                        modifier &= ~MODIFIER_LSHIFT;
                    break;

                    case CODE_RSHIFT:
                        modifier |= MODIFIER_RSHIFT;
                    break;

                    case CODE_RSHIFT | 0x80:
                        modifier &= ~MODIFIER_RSHIFT;
                    break;

                    case CODE_LCTRL:
                        modifier |= MODIFIER_LCTRL;
                    break;

                    case CODE_LCTRL | 0x80:
                        modifier &= ~MODIFIER_LCTRL;
                    break;

                    default: {
                        if (modifier & (MODIFIER_LSHIFT | MODIFIER_RSHIFT)) {
                            if (sc < sizeof(scan_codes_shift) / sizeof(scan_codes_shift[0]) && !scan_codes_shift[sc].empty()) {
                                const auto& u8str = scan_codes_shift[sc];
                                sv = string_view((char*)u8str.data(), u8str.length());
                            }
                        } else {
                            if (sc < sizeof(scan_codes) / sizeof(scan_codes[0]) && !scan_codes[sc].empty()) {
                                const auto& u8str = scan_codes[sc];
                                sv = string_view((char*)u8str.data(), u8str.length());
                            }
                        }

                        break;
                    }
                }
            }

            if (!sv.empty())
                print(sv);
        }
    } while (true);
}

void init_keyboard() {
    auto pa = allocate_page();
    if (!pa) {
        print("allocate_page failed\n");
        return;
    }

    auto va = allocate_va(1);
    if (!va) {
        print("allocate_va failed\n");
        return;
    }

    if (!map_page(pa, va, mapping_type::read_write)) {
        print("map_page failed\n");
        return;
    }

    keyboard_buffer = (uint8_t*)va;

    keyboard_event = create_event(true);
    if (!keyboard_event) {
        print("create_event failed\n");
        return;
    }

    auto& obj = deref_handle(keyboard_event.get())->get();
    keyboard_event_obj = static_cast<event&>(obj);

    if (!add_kernel_thread(keyboard_event_recv)) {
        print("add_kernel_thread failed\n");
        return;
    }

    add_idt_entry(keyboard_interrupt, (void*)keyboard_isr, false, 0);

    auto local_apic_id = *(volatile uint32_t*)&local_apic[0x20];

    // IRQ 1 = keyboard
    add_irq_handler(1, keyboard_interrupt, local_apic_id);
}

extern "C"
void keyboard_isr_func(uint8_t code) {
    {
        lock_guard lock(keyboard_lock);

        keyboard_buffer[keyboard_buffer_write_pos] = code;

        keyboard_buffer_write_pos++;
        keyboard_buffer_write_pos &= 0xfff;
    }

    set_event(keyboard_event_obj->get());
}
