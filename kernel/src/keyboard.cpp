module;

#include "kernel.h"
#include "handle.h"
#include "spinlock.h"
import objects;
#include "process.h"
#include <ioctl.h>

export module keyboard;

import pipe;
import print;
import evdev;

using namespace std;

#define ENABLE_PROCESSED_INPUT 0x1 // FIXME

static const unsigned int keyboard_pipe_buffer_pages = 1;
static object_ref keyboard_pipe_obj;

// UK layout
static const u8string_view codes[] = {
    u8"", // KEY_RESERVED
    u8"\x1b", // KEY_ESC
    u8"1", // KEY_1
    u8"2", // KEY_2
    u8"3", // KEY_3
    u8"4", // KEY_4
    u8"5", // KEY_5
    u8"6", // KEY_6
    u8"7", // KEY_7
    u8"8", // KEY_8
    u8"9", // KEY_9
    u8"0", // KEY_0
    u8"-", // KEY_MINUS
    u8"=", // KEY_EQUAL
    u8"\b", // KEY_BACKSPACE
    u8"\t", // KEY_TAB
    u8"q", // KEY_Q
    u8"w", // KEY_W
    u8"e", // KEY_E
    u8"r", // KEY_R
    u8"t", // KEY_T
    u8"y", // KEY_Y
    u8"u", // KEY_U
    u8"i", // KEY_I
    u8"o", // KEY_O
    u8"p", // KEY_P
    u8"[", // KEY_LEFTBRACE
    u8"]", // KEY_RIGHTBRACE
    u8"\n", // KEY_ENTER
    u8"", // KEY_LEFTCTRL
    u8"a", // KEY_A
    u8"s", // KEY_S
    u8"d", // KEY_D
    u8"f", // KEY_F
    u8"g", // KEY_G
    u8"h", // KEY_H
    u8"j", // KEY_J
    u8"k", // KEY_K
    u8"l", // KEY_L
    u8";", // KEY_SEMICOLON
    u8"'", // KEY_APOSTROPHE
    u8"`", // KEY_GRAVE
    u8"", // KEY_LEFTSHIFT
    u8"#", // KEY_BACKSLASH
    u8"z", // KEY_Z
    u8"x", // KEY_X
    u8"c", // KEY_C
    u8"v", // KEY_V
    u8"b", // KEY_B
    u8"n", // KEY_N
    u8"m", // KEY_M
    u8",", // KEY_COMMA
    u8".", // KEY_DOT
    u8"/", // KEY_SLASH
    u8"", // KEY_RIGHTSHIFT
    u8"*", // KEY_KPASTERISK
    u8"", // KEY_LEFTALT
    u8" ", // KEY_SPACE
    u8"", // KEY_CAPSLOCK
    u8"\x1bOP", // KEY_F1
    u8"\x1bOQ", // KEY_F2
    u8"\x1bOR", // KEY_F3
    u8"\x1bOS", // KEY_F4
    u8"\x1b[15~", // KEY_F5
    u8"\x1b[17~", // KEY_F6
    u8"\x1b[18~", // KEY_F7
    u8"\x1b[19~", // KEY_F8
    u8"\x1b[20~", // KEY_F9
    u8"\x1b[21~", // KEY_F10
    u8"", // KEY_NUMLOCK
    u8"", // KEY_SCROLLLOCK
    u8"7", // KEY_KP7
    u8"8", // KEY_KP8
    u8"9", // KEY_KP9
    u8"-", // KEY_KPMINUS
    u8"4", // KEY_KP4
    u8"5", // KEY_KP5
    u8"6", // KEY_KP6
    u8"+", // KEY_KPPLUS
    u8"1", // KEY_KP1
    u8"2", // KEY_KP2
    u8"3", // KEY_KP3
    u8"0", // KEY_KP0
    u8".", // KEY_KPDOT
    u8"",
    u8"", // KEY_ZENKAKUHANKAKU
    u8"\\", // KEY_102ND
    u8"\x1b[23~", // KEY_F11
    u8"\x1b[24~", // KEY_F12
    u8"", // KEY_RO
    u8"", // KEY_KATAKANA
    u8"", // KEY_HIRAGANA
    u8"", // KEY_HENKAN
    u8"", // KEY_KATAKANAHIRAGANA
    u8"", // KEY_MUHENKAN
    u8"", // KEY_KPJPCOMMA
    u8"\n", // KEY_KPENTER
    u8"", // KEY_RIGHTCTRL
    u8"/", // KEY_KPSLASH
    u8"", // KEY_SYSRQ
    u8"", // KEY_RIGHTALT
    u8"", // KEY_LINEFEED
    u8"\x1b[H", // KEY_HOME
    u8"\x1b[A", // KEY_UP
    u8"\x1b[5~", // KEY_PAGEUP
    u8"\x1b[D", // KEY_LEFT
    u8"\x1b[C", // KEY_RIGHT
    u8"\x1b[F", // KEY_END
    u8"\x1b[B", // KEY_DOWN
    u8"\x1b[6~", // KEY_PAGEDOWN
    u8"\x1b[2~", // KEY_INSERT
    u8"\x1b[3~", // KEY_DELETE
};

static const u8string_view codes_shift[] = {
    u8"", // KEY_RESERVED
    u8"\x1b", // KEY_ESC
    u8"!", // KEY_1
    u8"\"", // KEY_2
    u8"£", // KEY_3
    u8"$", // KEY_4
    u8"%", // KEY_5
    u8"^", // KEY_6
    u8"&", // KEY_7
    u8"*", // KEY_8
    u8"(", // KEY_9
    u8")", // KEY_0
    u8"_", // KEY_MINUS
    u8"+", // KEY_EQUAL
    u8"\b", // KEY_BACKSPACE
    u8"", // KEY_TAB
    u8"Q", // KEY_Q
    u8"W", // KEY_W
    u8"E", // KEY_E
    u8"R", // KEY_R
    u8"T", // KEY_T
    u8"Y", // KEY_Y
    u8"U", // KEY_U
    u8"I", // KEY_I
    u8"O", // KEY_O
    u8"P", // KEY_P
    u8"{", // KEY_LEFTBRACE
    u8"}", // KEY_RIGHTBRACE
    u8"", // KEY_ENTER
    u8"", // KEY_LEFTCTRL
    u8"A", // KEY_A
    u8"S", // KEY_S
    u8"D", // KEY_D
    u8"F", // KEY_F
    u8"G", // KEY_G
    u8"H", // KEY_H
    u8"J", // KEY_J
    u8"K", // KEY_K
    u8"L", // KEY_L
    u8":", // KEY_SEMICOLON
    u8"@", // KEY_APOSTROPHE
    u8"¬", // KEY_GRAVE
    u8"", // KEY_LEFTSHIFT
    u8"~", // KEY_BACKSLASH
    u8"Z", // KEY_Z
    u8"X", // KEY_X
    u8"C", // KEY_C
    u8"V", // KEY_V
    u8"B", // KEY_B
    u8"N", // KEY_N
    u8"M", // KEY_M
    u8"<", // KEY_COMMA
    u8">", // KEY_DOT
    u8"?", // KEY_SLASH
    u8"", // KEY_RIGHTSHIFT
    u8"*", // KEY_KPASTERISK
    u8"", // KEY_LEFTALT
    u8" ", // KEY_SPACE
    u8"", // KEY_CAPSLOCK
    u8"", // KEY_F1
    u8"", // KEY_F2
    u8"", // KEY_F3
    u8"", // KEY_F4
    u8"", // KEY_F5
    u8"", // KEY_F6
    u8"", // KEY_F7
    u8"", // KEY_F8
    u8"", // KEY_F9
    u8"", // KEY_F10
    u8"", // KEY_NUMLOCK
    u8"", // KEY_SCROLLLOCK
    u8"\x1b[H", // KEY_KP7 (home)
    u8"\x1b[A", // KEY_KP8 (up)
    u8"\x1b[5~", // KEY_KP9 (page up)
    u8"-", // KEY_KPMINUS
    u8"\x1b[D", // KEY_KP4 (left)
    u8"", // KEY_KP5
    u8"\x1b[C", // KEY_KP6 (right)
    u8"+", // KEY_KPPLUS
    u8"\x1b[F", // KEY_KP1 (end)
    u8"\x1b[B", // KEY_KP2 (down)
    u8"\x1b[6~", // KEY_KP3 (page down)
    u8"\x1b[2~", // KEY_KP0 (insert)
    u8"\x1b[3~", // KEY_KPDOT (delete)
    u8"",
    u8"", // KEY_ZENKAKUHANKAKU
    u8"|", // KEY_102ND
    u8"", // KEY_F11
    u8"", // KEY_F12
    u8"", // KEY_RO
    u8"", // KEY_KATAKANA
    u8"", // KEY_HIRAGANA
    u8"", // KEY_HENKAN
    u8"", // KEY_KATAKANAHIRAGANA
    u8"", // KEY_MUHENKAN
    u8"", // KEY_KPJPCOMMA
    u8"\n", // KEY_KPENTER
    u8"", // KEY_RIGHTCTRL
    u8"/", // KEY_KPSLASH
    u8"", // KEY_SYSRQ
    u8"", // KEY_RIGHTALT
    u8"", // KEY_LINEFEED
    u8"\x1b[H", // KEY_HOME
    u8"\x1b[A", // KEY_UP
    u8"\x1b[5~", // KEY_PAGEUP
    u8"\x1b[D", // KEY_LEFT
    u8"\x1b[C", // KEY_RIGHT
    u8"\x1b[F", // KEY_END
    u8"\x1b[B", // KEY_DOWN
    u8"\x1b[6~", // KEY_PAGEDOWN
    u8"\x1b[2~", // KEY_INSERT
    u8"\x1b[3~", // KEY_DELETE
};

static const u8string_view codes_ctrl[] = {
    u8"", // KEY_RESERVED
    u8"\x1b", // KEY_ESC
    u8"1", // KEY_1
    u8"2", // KEY_2
    u8"3", // KEY_3
    u8"4", // KEY_4
    u8"5", // KEY_5
    u8"6", // KEY_6
    u8"7", // KEY_7
    u8"8", // KEY_8
    u8"9", // KEY_9
    u8"0", // KEY_0
    u8"-", // KEY_MINUS
    u8"=", // KEY_EQUAL
    u8"\b", // KEY_BACKSPACE
    u8"\t", // KEY_TAB
    u8"\x11", // KEY_Q
    u8"\x17", // KEY_W
    u8"\x05", // KEY_E
    u8"\x12", // KEY_R
    u8"\x14", // KEY_T
    u8"\x19", // KEY_Y
    u8"\x15", // KEY_U
    u8"\x09", // KEY_I
    u8"\x0f", // KEY_O
    u8"\x10", // KEY_P
    u8"\x1b", // KEY_LEFTBRACE
    u8"\x1d", // KEY_RIGHTBRACE
    u8"\n", // KEY_ENTER
    u8"", // KEY_LEFTCTRL
    u8"\x01", // KEY_A
    u8"\x13", // KEY_S
    u8"\x04", // KEY_D
    u8"\x06", // KEY_F
    u8"\x07", // KEY_G
    u8"\x08", // KEY_H
    u8"\x0a", // KEY_J
    u8"\x0b", // KEY_K
    u8"\x0c", // KEY_L
    u8";", // KEY_SEMICOLON
    u8"'", // KEY_APOSTROPHE
    u8"\x00", // KEY_GRAVE
    u8"", // KEY_LEFTSHIFT
    u8"#", // KEY_BACKSLASH
    u8"\x1a", // KEY_Z
    u8"\x18", // KEY_X
    u8"\x03", // KEY_C
    u8"\x16", // KEY_V
    u8"\x02", // KEY_B
    u8"\x0e", // KEY_N
    u8"\x0d", // KEY_M
    u8",", // KEY_COMMA
    u8".", // KEY_DOT
    u8"\x1f", // KEY_SLASH
    u8"", // KEY_RIGHTSHIFT
    u8"*", // KEY_KPASTERISK
    u8"", // KEY_LEFTALT
    u8"\x00", // KEY_SPACE
    u8"", // KEY_CAPSLOCK
    u8"\x1bOP", // KEY_F1
    u8"\x1bOQ", // KEY_F2
    u8"\x1bOR", // KEY_F3
    u8"\x1bOS", // KEY_F4
    u8"\x1b[15~", // KEY_F5
    u8"\x1b[17~", // KEY_F6
    u8"\x1b[18~", // KEY_F7
    u8"\x1b[19~", // KEY_F8
    u8"\x1b[20~", // KEY_F9
    u8"\x1b[21~", // KEY_F10
    u8"", // KEY_NUMLOCK
    u8"", // KEY_SCROLLLOCK
    u8"7", // KEY_KP7
    u8"8", // KEY_KP8
    u8"9", // KEY_KP9
    u8"-", // KEY_KPMINUS
    u8"4", // KEY_KP4
    u8"5", // KEY_KP5
    u8"6", // KEY_KP6
    u8"+", // KEY_KPPLUS
    u8"1", // KEY_KP1
    u8"2", // KEY_KP2
    u8"3", // KEY_KP3
    u8"0", // KEY_KP0
    u8".", // KEY_KPDOT
    u8"",
    u8"", // KEY_ZENKAKUHANKAKU
    u8"\\", // KEY_102ND
    u8"\x1b[23~", // KEY_F11
    u8"\x1b[24~", // KEY_F12
    u8"", // KEY_RO
    u8"", // KEY_KATAKANA
    u8"", // KEY_HIRAGANA
    u8"", // KEY_HENKAN
    u8"", // KEY_KATAKANAHIRAGANA
    u8"", // KEY_MUHENKAN
    u8"", // KEY_KPJPCOMMA
    u8"\n", // KEY_KPENTER
    u8"", // KEY_RIGHTCTRL
    u8"/", // KEY_KPSLASH
    u8"", // KEY_SYSRQ
    u8"", // KEY_RIGHTALT
    u8"", // KEY_LINEFEED
    u8"\x1b[H", // KEY_HOME
    u8"\x1b[1;5A", // KEY_UP
    u8"\x1b[5~", // KEY_PAGEUP
    u8"\x1b[1;5D", // KEY_LEFT
    u8"\x1b[1;5C", // KEY_RIGHT
    u8"\x1b[F", // KEY_END
    u8"\x1b[1;5B", // KEY_DOWN
    u8"\x1b[6~", // KEY_PAGEDOWN
    u8"\x1b[2~", // KEY_INSERT
    u8"\x1b[3~", // KEY_DELETE
};

static const bool capslock_codes[] = {
    false, // KEY_RESERVED
    false, // KEY_ESC
    false, // KEY_1
    false, // KEY_2
    false, // KEY_3
    false, // KEY_4
    false, // KEY_5
    false, // KEY_6
    false, // KEY_7
    false, // KEY_8
    false, // KEY_9
    false, // KEY_0
    false, // KEY_MINUS
    false, // KEY_EQUAL
    false, // KEY_BACKSPACE
    false, // KEY_TAB
    true, // KEY_Q
    true, // KEY_W
    true, // KEY_E
    true, // KEY_R
    true, // KEY_T
    true, // KEY_Y
    true, // KEY_U
    true, // KEY_I
    true, // KEY_O
    true, // KEY_P
    false, // KEY_LEFTBRACE
    false, // KEY_RIGHTBRACE
    false, // KEY_ENTER
    false, // KEY_LEFTCTRL
    true, // KEY_A
    true, // KEY_S
    true, // KEY_D
    true, // KEY_F
    true, // KEY_G
    true, // KEY_H
    true, // KEY_J
    true, // KEY_K
    true, // KEY_L
    false, // KEY_SEMICOLON
    false, // KEY_APOSTROPHE
    false, // KEY_GRAVE
    false, // KEY_LEFTSHIFT
    false, // KEY_BACKSLASH
    true, // KEY_Z
    true, // KEY_X
    true, // KEY_C
    true, // KEY_V
    true, // KEY_B
    true, // KEY_N
    true, // KEY_M
    false, // KEY_COMMA
    false, // KEY_DOT
    false, // KEY_SLASH
    false, // KEY_RIGHTSHIFT
    false, // KEY_KPASTERISK
    false, // KEY_LEFTALT
    false, // KEY_SPACE
    false, // KEY_CAPSLOCK
    false, // KEY_F1
    false, // KEY_F2
    false, // KEY_F3
    false, // KEY_F4
    false, // KEY_F5
    false, // KEY_F6
    false, // KEY_F7
    false, // KEY_F8
    false, // KEY_F9
    false, // KEY_F10
    false, // KEY_NUMLOCK
    false, // KEY_SCROLLLOCK
    false, // KEY_KP7
    false, // KEY_KP8
    false, // KEY_KP9
    false, // KEY_KPMINUS
    false, // KEY_KP4
    false, // KEY_KP5
    false, // KEY_KP6
    false, // KEY_KPPLUS
    false, // KEY_KP1
    false, // KEY_KP2
    false, // KEY_KP3
    false, // KEY_KP0
    false, // KEY_KPDOT
    false,
    false, // KEY_ZENKAKUHANKAKU
    false, // KEY_102ND
    false, // KEY_F11
    false, // KEY_F12
    false, // KEY_RO
    false, // KEY_KATAKANA
    false, // KEY_HIRAGANA
    false, // KEY_HENKAN
    false, // KEY_KATAKANAHIRAGANA
    false, // KEY_MUHENKAN
    false, // KEY_KPJPCOMMA
    false, // KEY_KPENTER
    false, // KEY_RIGHTCTRL
    false, // KEY_KPSLASH
    false, // KEY_SYSRQ
    false, // KEY_RIGHTALT
    false, // KEY_LINEFEED
    false, // KEY_HOME
    false, // KEY_UP
    false, // KEY_PAGEUP
    false, // KEY_LEFT
    false, // KEY_RIGHT
    false, // KEY_END
    false, // KEY_DOWN
    false, // KEY_PAGEDOWN
    false, // KEY_INSERT
    false, // KEY_DELETE
};

static const bool numlock_codes[] = {
    false, // KEY_RESERVED
    false, // KEY_ESC
    false, // KEY_1
    false, // KEY_2
    false, // KEY_3
    false, // KEY_4
    false, // KEY_5
    false, // KEY_6
    false, // KEY_7
    false, // KEY_8
    false, // KEY_9
    false, // KEY_0
    false, // KEY_MINUS
    false, // KEY_EQUAL
    false, // KEY_BACKSPACE
    false, // KEY_TAB
    false, // KEY_Q
    false, // KEY_W
    false, // KEY_E
    false, // KEY_R
    false, // KEY_T
    false, // KEY_Y
    false, // KEY_U
    false, // KEY_I
    false, // KEY_O
    false, // KEY_P
    false, // KEY_LEFTBRACE
    false, // KEY_RIGHTBRACE
    false, // KEY_ENTER
    false, // KEY_LEFTCTRL
    false, // KEY_A
    false, // KEY_S
    false, // KEY_D
    false, // KEY_F
    false, // KEY_G
    false, // KEY_H
    false, // KEY_J
    false, // KEY_K
    false, // KEY_L
    false, // KEY_SEMICOLON
    false, // KEY_APOSTROPHE
    false, // KEY_GRAVE
    false, // KEY_LEFTSHIFT
    false, // KEY_BACKSLASH
    false, // KEY_Z
    false, // KEY_X
    false, // KEY_C
    false, // KEY_V
    false, // KEY_B
    false, // KEY_N
    false, // KEY_M
    false, // KEY_COMMA
    false, // KEY_DOT
    false, // KEY_SLASH
    false, // KEY_RIGHTSHIFT
    false, // KEY_KPASTERISK
    false, // KEY_LEFTALT
    false, // KEY_SPACE
    false, // KEY_CAPSLOCK
    false, // KEY_F1
    false, // KEY_F2
    false, // KEY_F3
    false, // KEY_F4
    false, // KEY_F5
    false, // KEY_F6
    false, // KEY_F7
    false, // KEY_F8
    false, // KEY_F9
    false, // KEY_F10
    false, // KEY_NUMLOCK
    false, // KEY_SCROLLLOCK
    true, // KEY_KP7
    true, // KEY_KP8
    true, // KEY_KP9
    false, // KEY_KPMINUS
    true, // KEY_KP4
    true, // KEY_KP5
    true, // KEY_KP6
    false, // KEY_KPPLUS
    true, // KEY_KP1
    true, // KEY_KP2
    true, // KEY_KP3
    true, // KEY_KP0
    true, // KEY_KPDOT
    false,
    false, // KEY_ZENKAKUHANKAKU
    false, // KEY_102ND
    false, // KEY_F11
    false, // KEY_F12
    false, // KEY_RO
    false, // KEY_KATAKANA
    false, // KEY_HIRAGANA
    false, // KEY_HENKAN
    false, // KEY_KATAKANAHIRAGANA
    false, // KEY_MUHENKAN
    false, // KEY_KPJPCOMMA
    false, // KEY_KPENTER
    false, // KEY_RIGHTCTRL
    false, // KEY_KPSLASH
    false, // KEY_SYSRQ
    false, // KEY_RIGHTALT
    false, // KEY_LINEFEED
    false, // KEY_HOME
    false, // KEY_UP
    false, // KEY_PAGEUP
    false, // KEY_LEFT
    false, // KEY_RIGHT
    false, // KEY_END
    false, // KEY_DOWN
    false, // KEY_PAGEDOWN
    false, // KEY_INSERT
    false, // KEY_DELETE
};

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

    uint32_t input_mode = ENABLE_PROCESSED_INPUT;

private:
    NTSTATUS console_set_foreground(span<const uint8_t> inbuf);

    spinlock fg_proc_lock;
    optional<reference_wrapper<process>> fg_proc;
};

export struct keyboard_leds {
    uint8_t caps_lock : 1;
    uint8_t num_lock : 1;
    uint8_t scroll_lock : 1;
    uint8_t : 5;
};

using set_leds_func = void (*)(void* ctx, keyboard_leds leds);

class keyboard_context {
public:
    keyboard_context(pipe& p, set_leds_func led_func, void* ctx) : p(p), led_func(led_func), ctx(ctx) {
        p.acquire();

        leds.caps_lock = 0;
        leds.num_lock = 0;
        leds.scroll_lock = 0;
    }

    ~keyboard_context() {
        p.release();
    }

    void listen_thread();

private:
    void key_event(input_event& ev);

    pipe& p;
    unsigned int shift_count = 0;
    unsigned int ctrl_count = 0;
    keyboard_leds leds;
    set_leds_func led_func;
    void* ctx;
};

export bool init_keyboard_pipe() {
    auto pptr = new keyboard_pipe(keyboard_pipe_buffer_pages);
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    keyboard_pipe_obj = object_ref{*pptr};

    if (!name_object(keyboard_pipe_obj.get(), "keyboard")) {
        print("name_object failed\n");
        return false;
    }

    return true;
}

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

void keyboard_context::key_event(input_event& ev) {
    auto idx = (unsigned int)ev.code;

    // FIXME - handle caps lock, num lock, etc.

    if (ev.code == evdev_code::KEY_LEFTSHIFT || ev.code == evdev_code::KEY_RIGHTSHIFT) {
        if (ev.value == 1) // key down
            shift_count++;
        else if (ev.value == 0) // key up
            shift_count--;

        return;
    } else if (ev.code == evdev_code::KEY_LEFTCTRL || ev.code == evdev_code::KEY_RIGHTCTRL) {
        if (ev.value == 1) // key down
            ctrl_count++;
        else if (ev.value == 0) // key up
            ctrl_count--;

        return;
    }

    if (ev.value == 0) // ignore key-ups
        return;

    if (ev.code == evdev_code::KEY_CAPSLOCK) {
        leds.caps_lock = leds.caps_lock ? 0 : 1;

        if (led_func)
            led_func(ctx, leds);

        return;
    } else if (ev.code == evdev_code::KEY_NUMLOCK) {
        leds.num_lock = leds.num_lock ? 0 : 1;

        if (led_func)
            led_func(ctx, leds);

        return;
    } else if (ev.code == evdev_code::KEY_SCROLLLOCK) {
        leds.scroll_lock = leds.scroll_lock ? 0 : 1;

        if (led_func)
            led_func(ctx, leds);

        return;
    }

    bool holding_shift = shift_count != 0;

    if (leds.caps_lock && idx < sizeof(capslock_codes) / sizeof(*capslock_codes) && capslock_codes[idx])
        holding_shift = !holding_shift;

    if (!leds.num_lock && idx < sizeof(numlock_codes) / sizeof(*numlock_codes) && numlock_codes[idx])
        holding_shift = true;

    u8string_view val;

    if (ctrl_count != 0) {
        if (idx >= sizeof(codes_ctrl) / sizeof(*codes_ctrl))
            return;

        if (codes_ctrl[idx].empty())
            return;

        val = codes_ctrl[idx];
    } else if (holding_shift) {
        if (idx >= sizeof(codes_shift) / sizeof(*codes_shift))
            return;

        if (codes_shift[idx].empty())
            return;

        val = codes_shift[idx];
    } else {
        if (idx >= sizeof(codes) / sizeof(*codes))
            return;

        if (codes[idx].empty())
            return;

        val = codes[idx];
    }

    auto sp = span((uint8_t*)val.data(), val.size());
    auto& k = static_cast<keyboard_pipe&>(keyboard_pipe_obj.get());

    if (!sp.empty() && sp.size() == 1 && sp[0] == '\x03' && k.input_mode & ENABLE_PROCESSED_INPUT) {
        k.handle_ctrl_c();
        return;
    }

    while (!sp.empty()) {
        uint64_t written_bytes;
        auto status = k.write(sp, written_bytes);

        if (!success(status)) {
            print("pipe::write returned {}\n", status);
            return;
        }

        if (written_bytes == sp.size())
            break;

        sp = span(sp.data() + written_bytes, sp.size() - written_bytes);
    }
}

NTSTATUS keyboard_pipe::console_set_foreground(span<const uint8_t> inbuf) {
    if (inbuf.size() != sizeof(HANDLE))
        return nt_status::INVALID_PARAMETER;

    auto h = *(HANDLE*)inbuf.data();

    auto obj = deref_handle(h);
    if (!obj)
        return nt_status::INVALID_HANDLE;

    if (obj.get().type != object_type::process)
        return nt_status::INVALID_HANDLE;

    auto& proc = static_cast<process&>(obj.get());

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

void keyboard_context::listen_thread() {
    if (led_func)
        led_func(ctx, leds);

    // FIXME - make sure this thread dies if keyboard goes away

    while (true) {
        input_event ev;
        nt_status status;

        auto sp = span((uint8_t*)&ev, sizeof(ev));

        while (true) {
            uint64_t read_bytes;

            status = p.read(sp, read_bytes);
            if (!success(status))
                break;

            if (read_bytes == sp.size())
                break;

            sp = span(sp.data() + read_bytes, sp.size() - read_bytes);
        }

        if (!success(status)) {
            print("pipe::read returned {}\n", status);
            break;
        }

        if (ev.type != evdev_type::EV_KEY)
            continue;

        key_event(ev);
    }
}

export void register_keyboard(pipe& p, set_leds_func led_func, void* ctx) {
    auto kc = new keyboard_context(p, led_func, ctx);
    if (!kc) {
        print("new failed\n");
        return;
    }

    add_kernel_thread([](void* ctx) {
        auto& kc = *(keyboard_context*)ctx;

        kc.listen_thread();

        delete &kc;
    }, kc);
}
