// SPDX-License-Identifier: GPL-2.0-only
module;

#include <stdint.h>
#include <string_view>
#include <span>
#include <utility>
#include <string.h>
#include <memory>
#include <xkbcommon/xkbcommon.h>

import nt;
import evdev;
import list;
import print;
import moxnox_ioctls;

export module keyboard;

using namespace std;

static const unsigned int XKB_EVDEV_OFFSET = 8;

class xkb_context_closer {
public:
    using pointer = xkb_context*;

    void operator()(xkb_context* ctx) {
        xkb_context_unref(ctx);
    }
};

export using unique_xkb_context = unique_ptr<xkb_context*, xkb_context_closer>;

class xkb_keymap_closer {
public:
    using pointer = xkb_keymap*;

    void operator()(xkb_keymap* ctx) {
        xkb_keymap_unref(ctx);
    }
};

export using unique_xkb_keymap = unique_ptr<xkb_keymap*, xkb_keymap_closer>;

class xkb_state_closer {
public:
    using pointer = xkb_state*;

    void operator()(xkb_state* ctx) {
        xkb_state_unref(ctx);
    }
};

export using unique_xkb_state = unique_ptr<xkb_state*, xkb_state_closer>;

class keyboard_context {
public:
    keyboard_context(uint32_t id, xkb_keymap* keymap);

    span<const char8_t> handle_keyboard_event(const input_event& ev, HANDLE pipe,
                                              bool& cad, bool decckm,
                                              span<char8_t> k);
    void set_led(HANDLE pipe, evdev_led_code led, int32_t value);

    list_entry le;
    uint32_t id;
    bool init = false;

private:
    unique_xkb_state state;
    xkb_keymap* keymap;
};

static list_entry contexts = { &contexts, &contexts };

keyboard_context::keyboard_context(uint32_t id, xkb_keymap* keymap) : id(id), keymap(keymap) {
    state.reset(xkb_state_new(keymap));
    if (!state) {
        print_stderr("xkb_state_new failed\n");
        return;
    }

    init = true;
}

span<const char8_t> keyboard_context::handle_keyboard_event(const input_event& ev,
                                                            HANDLE pipe, bool& cad,
                                                            bool decckm,
                                                            span<char8_t> k) {
    auto code = ev.code + XKB_EVDEV_OFFSET;

    if (ev.value != 2) {
        auto change = xkb_state_update_key(state.get(), code,
                                           ev.value == 0 ? XKB_KEY_UP : XKB_KEY_DOWN);

        if (change & XKB_STATE_LEDS) {
            auto caps_lock_led = xkb_keymap_led_get_index(keymap, XKB_LED_NAME_CAPS);

            if (caps_lock_led != XKB_LED_INVALID) {
                set_led(pipe, evdev_led_code::LED_CAPSL,
                        xkb_state_led_index_is_active(state.get(), caps_lock_led));
            }

            auto num_lock_led = xkb_keymap_led_get_index(keymap, XKB_LED_NAME_NUM);

            if (num_lock_led != XKB_LED_INVALID) {
                set_led(pipe, evdev_led_code::LED_NUML,
                        xkb_state_led_index_is_active(state.get(), num_lock_led));
            }

            auto scroll_lock_led = xkb_keymap_led_get_index(keymap, XKB_LED_NAME_SCROLL);

            if (scroll_lock_led != XKB_LED_INVALID) {
                set_led(pipe, evdev_led_code::LED_SCROLLL,
                        xkb_state_led_index_is_active(state.get(), scroll_lock_led));
            }
        }
    }

    if (ev.value == 0)
        return {};

    if (xkb_state_key_get_one_sym(state.get(), code) == XKB_KEY_Delete) {
        bool ctrl = xkb_state_mod_name_is_active(state.get(), XKB_MOD_NAME_CTRL,
                                                 XKB_STATE_MODS_EFFECTIVE);
        bool alt = xkb_state_mod_name_is_active(state.get(), XKB_MOD_NAME_ALT,
                                                XKB_STATE_MODS_EFFECTIVE);

        if (ctrl && alt) {
            cad = true;
            return {};
        }
    }

    auto sym = xkb_state_key_get_one_sym(state.get(), code);
    if (sym == XKB_KEY_NoSymbol)
        return {};

    u8string_view sv;
    span<const char8_t> val;

    switch (sym) {
        case XKB_KEY_Escape:
            sv = u8"\x1b";
            break;

        case XKB_KEY_F1:
            sv = u8"\x1bOP";
            break;

        case XKB_KEY_F2:
            sv = u8"\x1bOQ";
            break;

        case XKB_KEY_F3:
            sv = u8"\x1bOR";
            break;

        case XKB_KEY_F4:
            sv = u8"\x1bOS";
            break;

        case XKB_KEY_F5:
            sv = u8"\x1b[15~";
            break;

        case XKB_KEY_F6:
            sv = u8"\x1b[17~";
            break;

        case XKB_KEY_F7:
            sv = u8"\x1b[18~";
            break;

        case XKB_KEY_F8:
            sv = u8"\x1b[19~";
            break;

        case XKB_KEY_F9:
            sv = u8"\x1b[20~";
            break;

        case XKB_KEY_F10:
            sv = u8"\x1b[21~";
            break;

        case XKB_KEY_F11:
            sv = u8"\x1b[23~";
            break;

        case XKB_KEY_F12:
            sv = u8"\x1b[24~";
            break;

        case XKB_KEY_Home:
        case XKB_KEY_KP_Home:
            sv = u8"\x1b[H";
            break;

        case XKB_KEY_Up:
        case XKB_KEY_KP_Up:
            sv = xkb_state_mod_name_is_active(state.get(), XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) ? u8"\x1b[1;5A" : u8"\x1b[A";
            break;

        case XKB_KEY_Page_Up:
        case XKB_KEY_KP_Page_Up:
            sv = u8"\x1b[5~";
            break;

        case XKB_KEY_Left:
        case XKB_KEY_KP_Left:
            sv = xkb_state_mod_name_is_active(state.get(), XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) ? u8"\x1b[1;5D" : u8"\x1b[D";
            break;

        case XKB_KEY_Right:
        case XKB_KEY_KP_Right:
            sv = xkb_state_mod_name_is_active(state.get(), XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) ? u8"\x1b[1;5C" : u8"\x1b[C";
            break;

        case XKB_KEY_End:
        case XKB_KEY_KP_End:
            sv = u8"\x1b[F";
            break;

        case XKB_KEY_Down:
        case XKB_KEY_KP_Down:
            sv = xkb_state_mod_name_is_active(state.get(), XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) ? u8"\x1b[1;5B" : u8"\x1b[B";
            break;

        case XKB_KEY_Page_Down:
        case XKB_KEY_KP_Page_Down:
            sv = u8"\x1b[6~";
            break;

        case XKB_KEY_Insert:
        case XKB_KEY_KP_Insert:
            sv = u8"\x1b[2~";
            break;

        case XKB_KEY_Delete:
        case XKB_KEY_KP_Delete:
            sv = u8"\x1b[3~";
            break;

        case XKB_KEY_BackSpace:
            sv = u8"\x7f";
            break;

        default: {
            auto len = xkb_state_key_get_utf8(state.get(), code, (char*)k.data(), k.size());

            sv = u8string_view((const char8_t*)k.data(), (size_t)len);

            break;
        }
    }

    // return alternative forms for arrow keys, Home, and End in DECCKM mode
    if (decckm && sv.size() == 3 && sv[0] == 0x1b && sv[1] == '[' &&
        ((sv[2] >= 'A' && sv[2] <= 'D') || sv[2] == 'F' || sv[2] == 'H')) {
        switch (sv[2]) {
            case 'A':
                sv = u8"\x1bOA";
                break;

            case 'B':
                sv = u8"\x1bOB";
                break;

            case 'C':
                sv = u8"\x1bOC";
                break;

            case 'D':
                sv = u8"\x1bOD";
                break;

            case 'F':
                sv = u8"\x1bOF";
                break;

            case 'H':
                sv = u8"\x1bOH";
                break;

            default:
                unreachable();
        }
    }

    val = sv;

    return val;
}

void keyboard_context::set_led(HANDLE pipe, evdev_led_code led, int32_t value) {
    IO_STATUS_BLOCK iosb;
    input_event_with_id ev;

    ev.id = id;
    ev.ev.type = evdev_type::EV_LED;
    ev.ev.code = (evdev_code)led;
    ev.ev.value = value;

    auto status = NtWriteFile(pipe, 0, nullptr, nullptr, &iosb, &ev, sizeof(ev),
                              nullptr, nullptr);
    if (!success(status)) {
        print("NtWriteFile returned {}\n", status);
        return;
    }
}

export span<const char8_t> handle_keyboard_event(const input_event_with_id& ev,
                                                 xkb_keymap* keymap, HANDLE pipe,
                                                 bool& cad, bool decckm,
                                                 span<char8_t> k) {
    for (auto le = contexts.next; le != &contexts; le = le->next) {
        auto& ctx = containing_record<keyboard_context>(le);

        if (ctx.id == ev.id)
            return ctx.handle_keyboard_event(ev.ev, pipe, cad, decckm, k);
    }

    auto ctx = new keyboard_context(ev.id, keymap);
    if (!ctx) {
        print("new failed\n");
        return {};
    }

    insert_tail_list(contexts, ctx->le);

    ctx->set_led(pipe, evdev_led_code::LED_CAPSL, 0);
    ctx->set_led(pipe, evdev_led_code::LED_NUML, 0);
    ctx->set_led(pipe, evdev_led_code::LED_SCROLLL, 0);

    return ctx->handle_keyboard_event(ev.ev, pipe, cad, decckm, k);
}
