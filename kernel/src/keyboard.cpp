module;

#include "kernel.h"
#include "handle.h"

export module keyboard;

import pipe;
import print;
import evdev;

using namespace std;

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
    u8"", // KEY_KP7
    u8"", // KEY_KP8
    u8"", // KEY_KP9
    u8"-", // KEY_KPMINUS
    u8"", // KEY_KP4
    u8"", // KEY_KP5
    u8"", // KEY_KP6
    u8"+", // KEY_KPPLUS
    u8"", // KEY_KP1
    u8"", // KEY_KP2
    u8"", // KEY_KP3
    u8"", // KEY_KP0
    u8".", // KEY_KPDOT
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

class keyboard_context {
public:
    keyboard_context(pipe& p) : p(p) {
        p.acquire();
    }

    ~keyboard_context() {
        p.release();
    }

    void listen_thread();

private:
    void key_event(input_event& ev);

    pipe& p;
    unsigned int shift_count = 0;
};

export bool init_keyboard_pipe() {
    auto pptr = new pipe(keyboard_pipe_buffer_pages);
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

void keyboard_context::key_event(input_event& ev) {
    auto& k = static_cast<pipe&>(keyboard_pipe_obj.get());

    auto idx = (unsigned int)ev.code;

    // FIXME - handle ctrl, caps lock, num lock, etc.

    if (ev.code == evdev_code::KEY_LEFTSHIFT || ev.code == evdev_code::KEY_RIGHTSHIFT) {
        if (ev.value == 1) // key down
            shift_count++;
        else if (ev.value == 0) // key up
            shift_count--;

        return;
    }

    if (ev.value == 0) // ignore key-ups
        return;

    u8string_view val;

    if (shift_count != 0) {
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

void keyboard_context::listen_thread() {
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

export void register_keyboard(pipe& p) {
    auto kc = new keyboard_context(p);
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
