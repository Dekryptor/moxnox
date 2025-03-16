module;

#include "kernel.h"
#include "objects.h"
#include "spinlock.h"
#include "qwerty.h"
#include "handle.h"
#include "console.h"
#include <ioctl.h>

export module keyboard;

import print;
import inout;
import mem;

using namespace std;

static const uint8_t keyboard_interrupt = 0x31;
static const unsigned int keyboard_buffer_pages = 1;

static optional<reference_wrapper<event>> keyboard_event_obj;
static object_ref keyboard_obj;

static const unsigned int MODIFIER_LSHIFT = 1;
static const unsigned int MODIFIER_RSHIFT = 2;
static const unsigned int MODIFIER_EXTD = 4;
static const unsigned int MODIFIER_LCTRL = 8;
static const unsigned int MODIFIER_RCTRL = 16;

static spinlock keyboard_lock;
static uint8_t* keyboard_buffer;
static unsigned int keyboard_buffer_write_pos = 0;
static unsigned int keyboard_buffer_read_pos = 0;
static unsigned int modifier = 0;

static uint32_t input_mode = ENABLE_PROCESSED_INPUT;

static bool keyboard_initialized = false;
static uint16_t keyboard_port1;

class keyboard_pipe : public pipe {
public:
    keyboard_pipe(unsigned int buffer_pages) : pipe(buffer_pages) { }

    ~keyboard_pipe() {
        lock_guard lg(fg_proc_lock);

        if (fg_proc)
            fg_proc->get().release();
    }

    NTSTATUS ioctl(uint32_t code, span<const uint8_t> inbuf, span<uint8_t> outbuf, uintptr_t& written) override;
    void handle_ctrl_c();

private:
    NTSTATUS console_set_foreground(span<const uint8_t> inbuf);

    spinlock fg_proc_lock;
    optional<reference_wrapper<process>> fg_proc;
};

void keyboard_pipe::handle_ctrl_c() {
    optional<reference_wrapper<process>> proc;

    {
        lock_guard lg(fg_proc_lock);

        if (!fg_proc)
            return;

        proc = fg_proc;
        proc->get().acquire();
    }

    proc->get().send_signal((uint64_t)nt_status::CONTROL_C_EXIT);

    proc->get().release();
}

static void keyboard_event_recv() {
    do {
        wait_for_object(keyboard_event_obj->get(), false);

        lock_guard lock(keyboard_lock);

        while (keyboard_buffer_read_pos != keyboard_buffer_write_pos) {
            auto sc = keyboard_buffer[keyboard_buffer_read_pos];
            string_view sv;

            keyboard_buffer_read_pos++;
            keyboard_buffer_read_pos &= 0xfff;

            // FIXME - caps lock and num lock (and lights?)
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
                        if (modifier & (MODIFIER_LCTRL | MODIFIER_RCTRL))
                            sv = "\x1b[1;5A";
                        else
                            sv = "\x1b[A";
                    break;

                    case CODE_MODIFIED_PGUP:
                        sv = "\x1b[5~";
                    break;

                    case CODE_MODIFIED_LEFT:
                        if (modifier & (MODIFIER_LCTRL | MODIFIER_RCTRL))
                            sv = "\x1b[1;5D";
                        else
                            sv = "\x1b[D";
                    break;

                    case CODE_MODIFIED_RIGHT:
                        if (modifier & (MODIFIER_LCTRL | MODIFIER_RCTRL))
                            sv = "\x1b[1;5C";
                        else
                            sv = "\x1b[C";
                    break;

                    case CODE_MODIFIED_END:
                        sv = "\x1b[F";
                    break;

                    case CODE_MODIFIED_DOWN:
                        if (modifier & (MODIFIER_LCTRL | MODIFIER_RCTRL))
                            sv = "\x1b[1;5B";
                        else
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
                        if (modifier & (MODIFIER_LCTRL | MODIFIER_RCTRL)) {
                            if (sc < sizeof(scan_codes_ctrl) / sizeof(scan_codes_ctrl[0]) && !scan_codes_ctrl[sc].empty()) {
                                const auto& u8str = scan_codes_ctrl[sc];
                                sv = string_view((char*)u8str.data(), u8str.length());
                            }
                        } else if (modifier & (MODIFIER_LSHIFT | MODIFIER_RSHIFT)) {
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

            if (!sv.empty()) {
                auto& p = static_cast<keyboard_pipe&>(keyboard_obj.get());

                if (input_mode & ENABLE_PROCESSED_INPUT && sv.size() == 1 && sv[0] == '\x03')
                    p.handle_ctrl_c();
                else {
                    uint64_t written_bytes;

                    p.write(sv, written_bytes);
                }
            }
        }
    } while (true);
}

export void init_keyboard(uint16_t port1, uint16_t port2) {
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

    keyboard_event_obj = static_cast<event&>(keyboard_event.get());
    keyboard_event_obj->get().acquire();

    if (!add_kernel_thread(keyboard_event_recv)) {
        print("add_kernel_thread failed\n");
        return;
    }

    auto pptr = new keyboard_pipe(keyboard_buffer_pages);
    if (!pptr) {
        print("new failed\n");
        return;
    }

    keyboard_obj = object_ref{*pptr};

    if (!name_object(keyboard_obj.get(), "keyboard")) {
        print("name_object failed\n");
        return;
    }

    __asm __volatile("cli");

    while (inb(port2) & 1) {
        inb(port1);
    }

    add_idt_entry(keyboard_interrupt, (void*)keyboard_isr, false, 0, false);

    auto local_apic_id = *(volatile uint32_t*)&local_apic[0x20];

    // IRQ 1 = keyboard
    add_irq_handler(1, keyboard_interrupt, local_apic_id);

    __asm __volatile("sti");

    keyboard_port1 = port1;
    keyboard_initialized = true;
}

extern "C"
void keyboard_isr_func() {
    auto code = inb(keyboard_port1);

    {
        lock_guard lock(keyboard_lock);

        keyboard_buffer[keyboard_buffer_write_pos] = code;

        keyboard_buffer_write_pos++;
        keyboard_buffer_write_pos &= 0xfff;
    }

    set_event(keyboard_event_obj->get());
}

NTSTATUS keyboard_pipe::console_set_foreground(span<const uint8_t> inbuf) {
    if (inbuf.size() != sizeof(HANDLE))
        return nt_status::INVALID_PARAMETER;

    auto h = *(HANDLE*)inbuf.data();

    auto obj = deref_handle2(h);
    if (!obj)
        return nt_status::INVALID_HANDLE;

    optional<reference_wrapper<process>> procrw;

    visit_obj(obj.get(), [&](auto&& o) {
        if constexpr (is_same_v<remove_reference_t<decltype(o)>, process>)
            procrw = o;
    });

    if (!procrw.has_value())
        return nt_status::INVALID_HANDLE;

    auto& proc = procrw->get();
    proc.acquire();

    {
        lock_guard lg(fg_proc_lock);

        if (fg_proc)
            fg_proc->get().release();

        fg_proc = proc;
    }

    return nt_status::SUCCESS;
}

NTSTATUS keyboard_pipe::ioctl(uint32_t code, span<const uint8_t> inbuf,
                              span<uint8_t>, uintptr_t&) {
    switch (code) {
        case IOCTL_MOXNOX_CONSOLE_SET_FOREGROUND:
            return console_set_foreground(inbuf);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}
